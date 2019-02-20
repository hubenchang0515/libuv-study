#include <uv.h>
#include <stdio.h>

#ifndef STDOUT_FILENO 1
#define STDOUT_FILENO 1
#endif

/* 文件操作请求 */
uv_fs_t open_req;
uv_fs_t read_req;
uv_fs_t write_req;

/* 缓冲区 */
char buffer[10];
uv_buf_t io;

/* 函数声明 */
void onOpen(uv_fs_t* req);
void onRead(uv_fs_t* req);
void onWrite(uv_fs_t* req);


int main(int argc, char* argv[])
{

    uv_fs_open(uv_default_loop(), &open_req, argv[1], O_RDONLY, 0, onOpen);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    
    uv_fs_req_cleanup(&open_req);
    uv_fs_req_cleanup(&read_req);
    uv_fs_req_cleanup(&write_req);
    uv_loop_close(uv_default_loop());
    return 0;
}

/* 打开操作完成时调用的回调函数 */
void onOpen(uv_fs_t* req)
{
    if(req->result >=0)
    {
        io = uv_buf_init(buffer, sizeof(buffer));
        uv_fs_read(uv_default_loop(), &read_req, req->result, &io, 1, -1, onRead);
    }
}

/* 读操作完成时调用的回调函数 */
void onRead(uv_fs_t* req)
{
    if(req->result < 0)
    {
        fprintf(stderr, "Read error : %s\n", uv_strerror(req->result));
    }
    else if(req->result == 0)
    {
        uv_fs_t close_req;
        uv_fs_close(uv_default_loop(), &close_req, open_req.result, NULL);
    }
    else
    {
        io = uv_buf_init(buffer, req->result);
        uv_fs_write(uv_default_loop(), &write_req, STDOUT_FILENO, &io, 1, -1, onWrite);
    }
}

/* 写操作完成时调用的回调函数 */
void onWrite(uv_fs_t* req)
{
    uv_fs_read(uv_default_loop(), &read_req, open_req.result, &io, 1, -1, onRead);
}
