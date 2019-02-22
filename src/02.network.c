#include <uv.h>

/* 端口号 */
#ifndef HTTP_PORT 
#define HTTP_PORT 80
#endif 

/* 函数声明 */
void onNewConnection(uv_stream_t *server, int status);
void allocer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void readSocket(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void response(uv_stream_t *stream, const void* request, size_t length);
void readFileAfterOpen(uv_fs_t* req);
void readFileAfterWrite(uv_write_t* req, int state);
void writeSocket(uv_fs_t* req);
void closeSocket(uv_write_t* req, int status);

/* emmmmmmmmmmmm */
typedef void* ptr_t;

int main()
{
    /* 默认的事件循环 */
    uv_loop_t* loop = uv_default_loop();

    /* 创建socket */
    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    /* 绑定地址和端口 */
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", HTTP_PORT, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    /* 进行监听并注册回调函数 */
    int result = uv_listen((uv_stream_t*)&server, 10, onNewConnection);
    if(result < 0)
    {
        fprintf(stderr, "Listen error : %s\n", uv_strerror(result));
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}

/* 监听到新的连接时调用的回调函数 */
void onNewConnection(uv_stream_t *server, int status)
{
    if(status < 0)
    {
        fprintf(stderr, "Connection error : %s\n", uv_strerror(status));
        return;
    }

    /* 默认的事件循环 */
    uv_loop_t* loop = uv_default_loop();

    /* 创建Socket */
    uv_tcp_t* connection = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, connection);

    /* 将socket与连接绑定 */
    uv_accept(server, (uv_stream_t*) connection);

    /* 进行读取操作并绑定回调函数 */
    uv_read_start((uv_stream_t*) connection, allocer, readSocket);
    
}

/* 进行内存分配的回调函数 */
void allocer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

char http_response[] = "HTTP/1.1 200 OK\n\n<h1>hello world</h1>";
/* 读取操作完成时的回调函数 */
void readSocket(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    if(nread < 0 && nread != UV_EOF) // 出错
    {
        fprintf(stderr, "Read error : %s\n", uv_strerror(nread));
        uv_close((uv_handle_t*)stream, NULL);
    }
    else if(nread == UV_EOF) // 读到结尾
    {
        uv_close((uv_handle_t*)stream, NULL);
    }
    else if(nread > 0 && nread < buf->len) // 读完了内容
    {
        response(stream, buf->base, nread);
        //uv_close((uv_handle_t*)stream, NULL);
    }
    else // nread == buf->len 仍有未读完的内容
    {
        fprintf(stderr, "Long message is not supported.\n");
        uv_close((uv_handle_t*)stream, NULL);
    }

    free(buf->base);
    
}


/* HTTP应答 */
void response(uv_stream_t *stream, const void* request, size_t length)
{
    char url[128] = {'.',0};
    sscanf(request, "GET %s HTTP/1.1", url+1);
    if(url[strlen(url)-1] == '/')
    {
        strcat(url,"index.html");
    }
    //printf("%s\n", request);

    uv_fs_t* open_req = malloc(sizeof(uv_fs_t));
    open_req->data = stream;
    uv_fs_open(uv_default_loop(), open_req, url, O_RDONLY, 0, readFileAfterOpen);
}


/* 打开文件完成时调用,进行文件读取 */
void readFileAfterOpen(uv_fs_t* req)
{
    if(req->result < 0)
    {
        fprintf(stderr, "Open file error : %s\n", uv_strerror(req->result));
        uv_close(req->data, NULL);
        free(req);
        return;
    }

    uv_fs_t* read_req = malloc(sizeof(uv_fs_t));
    uv_buf_t* buf = malloc(sizeof(uv_buf_t));
    buf->base = malloc(1024);
    buf->len = 1024;
    ptr_t* dataArray = malloc(3 * sizeof(ptr_t));
    dataArray[0] = req->data; // socket
    dataArray[1] = buf; // buffer
    dataArray[2] = (void*)req->result; // fd
    read_req->data = (void*)dataArray;
    uv_fs_read(uv_default_loop(), read_req, req->result, buf, 1, -1, writeSocket);

    free(req);
}


/* 写完socket时调用，继续读取文件 */
void readFileAfterWrite(uv_write_t* req, int state)
{
    // if(req->result < 0)
    // {
    //     fprintf(stderr, "Write socket error : %s\n", uv_strerror(req->result));
    //     uv_close(req->data, NULL);
    //     free(req);
    //     return;
    // }

    ptr_t* dataArray = (ptr_t*)(req->data);
    uv_fs_t* read_req = malloc(sizeof(uv_fs_t));
    read_req->data = dataArray;
    uv_fs_read(uv_default_loop(), read_req, (int)dataArray[2], dataArray[1], 1, -1, writeSocket);

    free(req);
}

/* 文件读取完成时调用,写入socket */
void writeSocket(uv_fs_t* req)
{
    
    ptr_t* dataArray = (ptr_t*)(req->data);
    if(req->result < 0)
    {
        fprintf(stderr, "Read file error : %s\n", uv_strerror(req->result));
        uv_close(dataArray[0], NULL);
        free(req);
    }
    else if(req->result == 0)
    {
        uv_close(dataArray[0], NULL);
    }
    else
    {
        /* 将文件中读取到的内容发送回socket */
        //printf(" %d \n", req->result);
        uv_write_t* write_req = malloc(sizeof(uv_write_t)); // 
        write_req->data = dataArray;
        ((uv_buf_t*)dataArray[1])->len = req->result;
        uv_write(write_req, dataArray[0], dataArray[1], 1, readFileAfterWrite);
        //printf("html : %.*s\n", req->result, ((uv_buf_t*)dataArray[1])->base);
    }
}

/* 发送完成后关闭socket */
void closeSocket(uv_write_t* req, int status)
{
    ptr_t* dataArray = (ptr_t*)(req->data);
    //uv_close(dataArray[0], NULL); // 关闭socket

    free(dataArray[1]);  //  
    free(req); // 
}