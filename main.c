
#include <ctype.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "itop_sys.h"
#include "itop_app.h"
#include "itop_version.h"
#include "itop_http.h"
#include "itop_yuv.h"
#include "itop_mem.h"
#include "itop_log.h"
#include "event/itop_event_common.h"
#include "event/itop_event_pushCustom.h"
#include "itop_venc.h"
#include "config.h"
#include "itop_osd.h"
#include "itop_ai.h"
#include "object_ot.h"
#include "yolo_object.h"


#define ALIGN(value, align)   ((( (value) + ( (align) - 1 ) ) \
            / (align) ) * (align) )
#define FLOOR(value, align)   (( (value) / (align) ) * (align) )

#define APP_MAX_AI_RESULT_NUM   (32)



#define MAX_CLINET_NUMS 		(10)
#define PORT 				6698
char IPC_IP[16] =			"172.27.5.81";


struct app_global_t {
    int                         hNet;
    void*                   hYuv;
    void*                   hVenc;
    ITOP_AI_NNX_Handle          hNNX;

    int32_t                    resultNum;
    int32_t                    encAlignW;
    int32_t                    encAlignH;
    obj_info_t                  aiResult[APP_MAX_AI_RESULT_NUM];
};

struct app_global_t  		    g_app_global;
int32_t							g_clientFD[MAX_CLINET_NUMS];


typedef int32_t (*p_func)(const char* ipc_ip, uint32_t port);


struct paint_info_t {
    ITOP_OSD_Element            element[2];
    ITOP_OSD_Polygon            rect;
    ITOP_OSD_Point              points[4];
    ITOP_OSD_Text               text;
    char                     szText[24];
};

static struct app_config        g_app_config;



void setNonBlock(struct epoll_event *ev)
{
	int32_t flag;
	ev->events = EPOLLIN | EPOLLET;
	flag = fcntl(ev->data.fd, F_GETFL);          /* 修改connfd为非阻塞??*/  
    flag |= O_NONBLOCK;  
    fcntl(ev->data.fd, F_SETFL, flag); 
}

void* serverStart(void* arg)
{
    struct epoll_event tmp,ep[5];//創建epoll的對??
    int lfd,cfd;
    ssize_t n;
    char buf[BUFSIZ],clien_IP[16];
    struct sockaddr_in serv_addr,client_addr;

    lfd = socket(AF_INET,SOCK_STREAM,0);
	if (lfd < 0)
	{
		while(1)
		{
			ITOP_LOG_INFO("----- sock fail -----\n");
		}
	}
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_family = AF_INET;
	inet_ntop(AF_INET, &serv_addr.sin_addr, IPC_IP, sizeof(IPC_IP));
    int opt = 1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);

    if(bind(lfd,(struct sockaddr *)&serv_addr,sizeof serv_addr) == -1)//
    {
        perror("bind error\n");
    }
    listen(lfd,2);

    //創建epollfd，即是紅黑樹的根節
    int epfd = epoll_create(5);
    if(epfd == -1)
    {
        perror("epoll_creat error!!\n");
        return NULL;
    }
    
	tmp.data.fd = lfd;
	
	//设置非阻??
	setNonBlock(&tmp);
	
    if(-1 == epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&tmp))
    {
            perror("epoll_ctl error!!!\n");
            return NULL;
    }
	ITOP_LOG_INFO("-----server start listen -----%#x\n");
    while(1)
    {
        int res = epoll_wait(epfd,ep,5,-1);
        if(res == -1)
        {
            perror("wait shibai");
			return NULL;
        }
        int ii = 0;
        for (ii = 0; ii < res; ii++)
        {
            if (!(ep[ii].events & EPOLLIN))
            {
                continue;//如果不是读事件直接跳??
            }
			
            if (ep[ii].data.fd == lfd)
            {
                socklen_t client_addr_size = sizeof client_addr;
                cfd = accept(lfd,(struct sockaddr *)&client_addr,&client_addr_size);
                ITOP_LOG_INFO("client IP:%s,Port:%d\n",inet_ntop(AF_INET,&client_addr.sin_addr.s_addr,clien_IP,sizeof clien_IP),ntohs(client_addr.sin_port));
				tmp.data.fd = cfd;
				setNonBlock(&tmp);
                int res = epoll_ctl(epfd,EPOLL_CTL_ADD,cfd, &tmp);
                if(res == -1)
                {
                    perror("epoll_ctl error");
                    break;
                }
				
				int k;
				for(k=0; k<MAX_CLINET_NUMS; k++)
				{
					//没做已连接数量判??
					if( g_clientFD[k]<= 0 )
					{
						g_clientFD[k]	=	cfd;
						break;
					}
				}
			}
            else
			{
				//读事件响应，根据??返回字段判断类型做处??
				char buff[BUFSIZ];
				memset(buff,0,sizeof(BUFSIZ));
                int res = read(ep[ii].data.fd,buff,sizeof(buff));
				if(-1 == res)
				{
					if(res == EAGAIN || res == EINTR ){
						continue;
					}else
					{
						//客户端断开
						ITOP_LOG_INFO("客户端断开\n");
						int k;
						for(k=0; k<MAX_CLINET_NUMS; k++)
						{
							//回收客户端连接句??
							if(ep[ii].data.fd == g_clientFD[k] )
							{
								g_clientFD[k] = 0;
								ITOP_LOG_INFO("客户端句柄回收成功\n");
							}
						}
					}
				}else
				{
					//响应客户端发送的数据
					
				}
				
            }
		}
	}
	pthread_self();
}



// Convert AI detection coordinates
int32_t app_size_limit(int32_t pix, int32_t max)
{
    if (pix < 0)
    {
        return 0;
    }
    if(pix > max)
    {
        return max-1;
    }
    return pix;
}

int app_net_init() {
    int32_t                ret;
    struct sockaddr_in      sever_addr;

    g_app_global.hNet = socket(AF_INET, SOCK_STREAM, 0);
    if (g_app_global.hNet < 0)
    {
        perror("socket failed");
        g_app_global.hNet = -1;
        return -1;
    }
    ITOP_LOG_INFO("create socket %d\n", g_app_global.hNet);

    memset(&sever_addr, 0, sizeof(sever_addr));

    sever_addr.sin_family       = AF_INET;
    sever_addr.sin_port         = htons(g_app_config.net_port);
    sever_addr.sin_addr.s_addr  = inet_addr(g_app_config.net_ip);

    ret = connect(g_app_global.hNet, (struct sockaddr *)&sever_addr, sizeof(sever_addr));
    if (ret != 0)
    {
        perror("connect failed");

        close(g_app_global.hNet);
        g_app_global.hNet = -1;
    }
    else 
	{
        ITOP_LOG_INFO("connect success: %d\n", g_app_global.hNet);
    }

    return ret;
}

int app_net_deinit() {
    if (g_app_global.hNet > 0) {
        close(g_app_global.hNet);
        g_app_global.hNet = -1;
    }
    return 0;
}

int app_net_reinit() {
    app_net_deinit();
    return app_net_init();
}

int app_enc_init() {
    int32_t                ret;
    ITOP_VENC_CapsInfo      encCap;
    ITOP_VENC_CreateParam   createParam;
	const ITOP_YUV_CapSet*		capSet;

    ret = ITOP_YUV_getCapSet(&capSet);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_YUV_getCapSet fail with %#x\n", ret);
        return ret;
    }

    memset(&createParam, 0, sizeof(createParam));
    createParam.cbSize = sizeof(createParam);
    createParam.type = ITOP_VENC_YUV_TO_JPEG;
    createParam.max_width = capSet->chnList[0].vpssList[0].maxWidth;
    createParam.max_height = capSet->chnList[0].vpssList[0].maxHeight;

    ret = ITOP_VENC_getCaps(&encCap);

    g_app_global.encAlignW = encCap.alignWidth;
    g_app_global.encAlignH = encCap.alignHeight;
    if (encCap.yuvFmtMask & (1 << ITOP_YUV_FMT_420P_I420))
    {
        createParam.format = ITOP_YUV_FMT_420P_I420;
    }
    else if (encCap.yuvFmtMask & (1 << ITOP_YUV_FMT_420SP_VU))
    {
        createParam.format = ITOP_YUV_FMT_420SP_VU;
    }
    else
    {
        createParam.format = ITOP_YUV_FMT_420SP_UV;
    }

    // Create encoder
    ret = ITOP_VENC_create(&createParam, &(g_app_global.hVenc));
    if (ret != ITOP_SUCCESS) {
        ITOP_LOG_ERROR("ITOP_VENC_create fail with %#x\n", ret);
        return ret;
    }

    return 0;
}

int app_enc_deinit() {
    ITOP_VENC_destroy(&(g_app_global.hVenc));
    return 0;
}

// Initialization of YUV
int32_t app_yuv_init()
{
    int32_t                ret = -1;
    ITOP_YUV_Option         yuvOption;
    ITOP_YUV_OpenParam      yuvOpenPrm;
    int32_t                yuvChn = 0;
	
	const ITOP_YUV_CapSet *pCaps;
	ret = ITOP_YUV_getCapSet(&pCaps);
	if(0 != ret)
    {
        ITOP_LOG_ERROR("ITOP_YUV_getCapSet fail with %#x\n", ret);
        return ret;
    }
    ITOP_LOG_INFO("ITOP_YUV_getCapSet  success");

    memset(&yuvOpenPrm, 0, sizeof(yuvOpenPrm));
    yuvOpenPrm.channel = 0;
    yuvOpenPrm.fps=pCaps->chnList[0].vpssList[0].maxFps;
	yuvOpenPrm.width=pCaps->chnList[0].vpssList[0].maxWidth;
	yuvOpenPrm.height=pCaps->chnList[0].vpssList[0].maxHeight;
    if (pCaps->chnList[0].vpssList[0].fmtMask & (1 << ITOP_YUV_FMT_420P_I420))
    {
        yuvOpenPrm.format = ITOP_YUV_FMT_420P_I420;
    }
    else if(pCaps->chnList[0].vpssList[0].fmtMask&(1 << ITOP_YUV_FMT_420SP_VU))
    {
        yuvOpenPrm.format = ITOP_YUV_FMT_420SP_VU;
    }
    else
    {
        yuvOpenPrm.format = ITOP_YUV_FMT_420SP_UV;
    }
    // 3.Set YUV channel format parameters
    ret = ITOP_YUV_open(&yuvOpenPrm, &(g_app_global.hYuv));
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_YUV_open fail with %#x\n", ret);
        return ret;
    }

    memset(&yuvOption, 0, sizeof(yuvOption));
    yuvOption.cbSize = sizeof(yuvOption);
    yuvOption.type = ITOP_YUV_OPT_DEPTH;
    yuvOption.option.depth = 1;
    // 4.Set YUV optional configuration
    ret = ITOP_YUV_setOption(g_app_global.hYuv, &yuvOption);
    if(ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_YUV_setOption fail with %#x\n", ret);
        ITOP_YUV_close(&g_app_global.hYuv);
        return ret;
    }
    return 0;
}

// Close YUV
int32_t app_yuv_deinit()
{
    ITOP_YUV_close(&(g_app_global.hYuv));
    return ITOP_SUCCESS;
}

// Draw frame
int32_t app_result_draw()
{
    int                     i;
    struct paint_info_t    *info;
    ITOP_OSD_PaintWork      work[APP_MAX_AI_RESULT_NUM];

    if (g_app_global.resultNum <= 0)
    {
        return ITOP_FAILED;
    }

    info = (struct paint_info_t*)malloc(APP_MAX_AI_RESULT_NUM * sizeof(struct paint_info_t));
    if (info == NULL) {
        return ITOP_FAILED;
    }
    memset(info, 0, APP_MAX_AI_RESULT_NUM * sizeof(struct paint_info_t));

    for (i = 0; i < g_app_global.resultNum; i++)
    {
        work[i].chn = 0;
        work[i].id  = i+1;
        work[i].pts = 0;
        work[i].coustomerField = 1;
        work[i].state = 0;
        work[i].numOfElem = 2;

        //draw text

        info[i].text.type           = ITOP_OSD_ET_TEXT;
        info[i].text.charset        = ITOP_OSD_CS_UTF8;
        info[i].text.fontsize       = 30;
        info[i].text.position.x     = g_app_global.aiResult[i].rect.lt.x;
        info[i].text.position.y     = g_app_global.aiResult[i].rect.lt.y;
        info[i].text.color.A        = 255;
        info[i].text.color.R        = 0;
        info[i].text.color.G        = 255;
        info[i].text.color.B        = 0;
        sprintf(info[i].szText, "%s", g_yolo_object_list[g_app_global.aiResult[i].classId]);
        info[i].text.length         = strlen(info[i].szText)+1;
        info[i].text.text           = info[i].szText;
        info[i].element[0].text     = &info[i].text;

        //draw coordinates

        info[i].rect.type           = ITOP_OSD_ET_POLYGON;
        info[i].rect.borderWeight   = 2;
        info[i].rect.style          = ITOP_OSD_RS_SOLID;
        info[i].rect.clrBorder.A    = 255;
        info[i].rect.clrBorder.R    = 255;
        info[i].rect.clrBorder.G    = 0;
        info[i].rect.clrBorder.B    = 0;
        info[i].rect.pointNum       = 4;

        info[i].points[0].x         = g_app_global.aiResult[i].rect.lt.x;
        info[i].points[0].y         = g_app_global.aiResult[i].rect.lt.y;
        info[i].points[1].x         = g_app_global.aiResult[i].rect.rb.x;
        info[i].points[1].y         = g_app_global.aiResult[i].rect.lt.y;
        info[i].points[2].x         = g_app_global.aiResult[i].rect.rb.x;
        info[i].points[2].y         = g_app_global.aiResult[i].rect.rb.y;
        info[i].points[3].x         = g_app_global.aiResult[i].rect.lt.x;
        info[i].points[3].y         = g_app_global.aiResult[i].rect.rb.y;
        info[i].rect.points         = info[i].points;
        info[i].element[1].region   = &info[i].rect;

        work[i].elems = info[i].element;
    }

    int ret = ITOP_OSD_richPaint(work, g_app_global.resultNum);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_INFO("ITOP_OSD_richPaint fail with %#x\n", ret);
    }

    free(info);

    return ret;
}

// Send the detection results to the server
int app_result_send(ITOP_VENC_Result* image, obj_info_t * result) {
    int32_t                ret = -1;
    struct {
        int                 cbsize;
        int                 pic_size;
        short               lt_x;
        short               lt_y;
        short               rb_x;
        short               rb_y;
        unsigned int        magic_code; // 0x12345678
        char                name[48];
    } img_info;

    if (g_app_global.hNet > 0) {

        memset(&img_info, 0, sizeof(img_info));
        img_info.cbsize     = sizeof(img_info);
        img_info.pic_size   = image->size;
        img_info.lt_x       = result->actual.lt.x;
        img_info.lt_y       = result->actual.lt.y;
        img_info.rb_x       = result->actual.rb.x;
        img_info.rb_y       = result->actual.rb.y;
        img_info.magic_code = 0x12345678;

        if (result->classId >= 0 && result->classId < 80) {
            strcpy(img_info.name, g_yolo_object_list[result->classId]);
        }
        else {
            return -1;
        }
        ret = send(g_app_global.hNet, &img_info, sizeof(img_info), 0);
        if (ret < 0) {
            perror("send head failed:");
            app_net_reinit();
            return -2;
        }

        ret = send(g_app_global.hNet, image->virAddr, image->size, 0);
        if (ret < 0) {
            perror("send image failed:");
            app_net_reinit();
            return -3;
        }

        return 0;
    }

    return -1;
}

// Encode the data detected by the algorithm
void app_result_snap(obj_info_t * result, ITOP_YUV_FrameData2* frame) {
    int32_t                ret = -1;
    ITOP_VENC_ReqInfo2      encReq;
    ITOP_VENC_Result        encResult;

    memset(&encReq, 0, sizeof(encReq));
    encReq.cbSize       = sizeof(encReq);
    encReq.quality      = ITOP_VENC_JPEG_QUALITY_DEFAULT;
    encReq.region.lt.x  = FLOOR(result->actual.lt.x, g_app_global.encAlignW);
    encReq.region.lt.y  = FLOOR(result->actual.lt.y, g_app_global.encAlignH);
    encReq.region.rb.x  = FLOOR(result->actual.rb.x, g_app_global.encAlignW);
    encReq.region.rb.y  = FLOOR(result->actual.rb.y, g_app_global.encAlignH);
    encReq.data         = frame;
    encReq.timeout      = 200;
    ret = ITOP_VENC_sendRequest(g_app_global.hVenc, &encReq);
    if (ret != ITOP_SUCCESS) {
        ITOP_LOG_WARN("Send enc reqeust failed!\n");
    }
    else {
        memset(&encResult, 0, sizeof(encResult));
        encResult.cbSize  = sizeof(encResult);
        encResult.pts     = encReq.data->pts;
        encResult.timeout = 200;
        ret = ITOP_VENC_getResult(g_app_global.hVenc, &encResult);
        if (ret != ITOP_SUCCESS) {
            ITOP_LOG_WARN("Get enc result failed!\n");
        }

        app_result_send(&encResult, result);

        ret = ITOP_VENC_releaseResult(g_app_global.hVenc, &encResult);
        if (ret != ITOP_SUCCESS) {
            ITOP_LOG_WARN("Release enc result failed!\n");
        }
    }
}

int app_ai_init() {

    int32_t                ret = -1;
    ITOP_AI_NNX_Version     ver;
    char modelkeystr[] = "gGQaZICol8tv2BqcbQ0UZG9kXmRvZBnsPWQDZMRkkmSg/G8xkNtvZG5k5a1vZG/81a00yXIgcMpvDpFkOmRMYg==";

    ver.byte_size = sizeof(ITOP_AI_NNX_Version);
    // Get engine version number
    ret = ITOP_AI_NNX_getVersion(&ver);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_AI_NNX_getVersion %#x\n", ret);
        return ITOP_FAILED;
    }
    ITOP_LOG_INFO("version:%s\n", ver.version);

    // AI initialization
    ret = ITOP_AI_init();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("itop ai init fail %#x\n", ret);
        return ITOP_FAILED;
    }
    ITOP_LOG_INFO("ITOP_AI_init success\n");

    // Create engine
    ret = ITOP_AI_NNX_create(&(g_app_global.hNNX), g_modelfile, modelkeystr);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("itop ai nnx create fail#x\n", ret);
        return ITOP_FAILED;
    }

    return 0;
}

int app_ai_deinit() {
    // Destroy algorithm model handle
    ITOP_AI_NNX_destroy(g_app_global.hNNX);
    // AI unInitialization
    ITOP_AI_unInit();

    return 0;
}

int32_t app_ai_process(ITOP_AI_NNX_Handle hNNX, ITOP_YUV_FrameData2 * frame)
{
    int32_t                ret = -1, i, k;
    void*                  ptrs[3];
    void*                  ptrs_HW[3];
    int32_t                strides[3];
    ITOP_AI_IMG_Handle      hImg;
    uint32_t               type = ITOP_AI_NNX_RESULT_TYPE_MAX;
    ITOP_AI_MAT_Handle      yoloMat;
    int32_t                h,start;
    ITOP_AI_NNX_ResultYolo *yolo_result;

    g_app_global.resultNum = 0;

    ptrs[0]     = frame->data.virAddr.i420.y;
    ptrs[1]     = frame->data.virAddr.i420.u;
    ptrs[2]     = frame->data.virAddr.i420.v;
    ptrs_HW[0]  = frame->data.phyAddr.i420.y;
    ptrs_HW[1]  = frame->data.phyAddr.i420.u;
    ptrs_HW[2]  = frame->data.phyAddr.i420.v;
    strides[0]  = frame->data.stride[0];
    strides[1]  = frame->data.stride[1];
    strides[2]  = frame->data.stride[2];

    // AI to initialize and create ITOP_ AI_ IMG_ Handle, Itop required_ AI_ IMG_ Destroy() to free img memory
    ret = ITOP_AI_IMG_create(&hImg,
                             frame->data.width,
                             frame->data.height,
                             ITOP_AI_IMG_CS_YUV420,
                             ITOP_AI_IMG_8U,
                             ptrs,
                             ptrs_HW,
                             strides,
                             3);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("creat ITOP_AI_IMG_Handle fail\n");
        goto err0;
    }

    // Set picture input
    ret = ITOP_AI_NNX_setInputImg(hNNX, (const char*)"image", &hImg, 1);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("itop ai set input img fail");
        goto err1;
    }

    // Run AI
    ret = ITOP_AI_NNX_run(hNNX);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("itop ai run fail");
        goto err1;
    }

    // Obtain AI test results
    type = ITOP_AI_NNX_RESULT_TYPE_MAX;
    ret = ITOP_AI_NNX_getResult(hNNX, (const char*)"result", &type, &yoloMat);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("itop ai get result fail");
        goto err1;
    }
    if (type != ITOP_AI_NNX_RESULT_TYPE_YOLO)
    {
        ITOP_LOG_ERROR("nnx result type is not yolo\n");
        goto err1;
    }

    // Gets the active range of the matrix on the specified dimension.
    ret = ITOP_AI_MAT_getActiveRange(yoloMat, 0, &start, &h);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("get rect num fail\n");
        goto err1;
    }

    yolo_result = (ITOP_AI_NNX_ResultYolo *)ITOP_AI_MAT_ptr2(yoloMat, NULL);
    if (NULL == yolo_result) {
        goto err1;
    }

    for (i = start, k = 0; i < start+h; i++)
    {
        // Filter confidence minima
        if ( yolo_result[i].prob > 0.1 )
        {
            if (k < APP_MAX_AI_RESULT_NUM) {
                // The output of the algorithm is 0 ~ 1 floating-point data, which should be converted into 8192 coordinates
                g_app_global.aiResult[k].rect.lt.x = app_size_limit((yolo_result[i].x - yolo_result[i].w/2) * 8192, 8192);
                g_app_global.aiResult[k].rect.lt.y = app_size_limit((yolo_result[i].y - yolo_result[i].h/2) * 8192, 8192);
                g_app_global.aiResult[k].rect.rb.x = app_size_limit((yolo_result[i].x + yolo_result[i].w/2) * 8192, 8192);
                g_app_global.aiResult[k].rect.rb.y = app_size_limit((yolo_result[i].y + yolo_result[i].h/2) * 8192, 8192);
                g_app_global.aiResult[k].classId   = yolo_result[i].classIdx;

                // The output of the algorithm is 0 ~ 1 floating-point data, which should be converted into the width and height coordinates of YUV frame
                g_app_global.aiResult[k].actual.lt.x = app_size_limit((yolo_result[i].x - yolo_result[i].w/2) * frame->data.width, frame->data.width);
                g_app_global.aiResult[k].actual.lt.y = app_size_limit((yolo_result[i].y - yolo_result[i].h/2) * frame->data.height, frame->data.height);
                g_app_global.aiResult[k].actual.rb.x = app_size_limit((yolo_result[i].x + yolo_result[i].w/2) * frame->data.width, frame->data.width);
                g_app_global.aiResult[k].actual.rb.y = app_size_limit((yolo_result[i].y + yolo_result[i].h/2) * frame->data.height, frame->data.height);

                k++;
            }
            else {
                break;
            }
        }
    }
    g_app_global.resultNum = k;

err1:
    /// Destroy ITOP_ AI_ IMG_ Handle handle
    ret = ITOP_AI_IMG_destroy(hImg);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_AI_IMG_destroy fail\n");
    }

err0:
    return ret;
}

typedef struct {
	char                name[48];
	short               w;
	short               h;
	uint8_t				*y;
	uint8_t				*u;
	uint8_t				*v;
} Package;


int32_t app_send(obj_info_t * result, ITOP_YUV_FrameData2* frame)
{
	if(frame->data.format == ITOP_YUV_FMT_420P_I420)
	{
		int i,ret;
		Package package;
		
		if (result->classId >= 0 && result->classId < 80) {
            strcpy(package.name, g_yolo_object_list[result->classId]);
        }
		package.w = result->rect.rb.x - result->rect.lt.x;
		package.h = result->rect.rb.y - result->rect.rb.y;
		int subPicSize = package.w * package.h * 1.5;
		int sizeY 	= sizeof(uint8_t) * package.w * package.h;
		int sizeUV 	= sizeof(uint8_t) * package.w * package.h * 0.25;
		
		package.y = (uint8_t *)malloc( sizeY );
		package.u = (uint8_t *)malloc( sizeUV );
		package.v = (uint8_t *)malloc( sizeUV );
		
		memset(package.y, 0, sizeY);
		memset(package.u, 0, sizeUV);
		memset(package.v, 0, sizeUV);
		
		int flag = 0;
		for(i =0; i<MAX_CLINET_NUMS; i++)
		{
			if( g_clientFD[i] > 0 )
			{
				//发送图像帧到客户端
				ret = write(g_clientFD[i], (void *)&package, sizeof(package));
				ITOP_LOG_INFO("send frame to client ---[class:%s %d %d ]\n", i, package.name, package.w, package.h);
				if(-1 == ret)
				{
					ITOP_LOG_INFO("write frame to client fail %d\n", i);
				}
				flag = 1;
			}
		}
		
		if (0 == flag)
		{
			ITOP_LOG_INFO("----no client connect----\n");
		}
		
		free(package.y);
		free(package.u);
		free(package.v);
		
		return ITOP_SUCCESS;
		
	}else
	{
		ITOP_LOG_INFO("-----YUV format is not I420-----");
		return ITOP_FAILED;

	}
	return ITOP_SUCCESS;
}

int32_t app_ai_task()
{
    int32_t                i, ret = -1;
    int32_t                tryCnt = 0;
    ITOP_YUV_FrameData2     yuvFrame;

    while(1)
    {
        if (g_app_config.linkage_event) {
            //The linkage snapshot is turned on, and the network configuration or linkage snapshot configuration has changed
            if (g_app_config.mask & (_CCM_BIT_NET_INFO | _CCM_BIT_LINKAGE)) {
                app_net_reinit();
                g_app_config.mask &= (~(_CCM_BIT_NET_INFO | _CCM_BIT_LINKAGE));
            }
        }
        else {
            // If the linkage snapshot is closed, the network will be stopped
            app_net_deinit();
        }
        // Log level changed
        if (g_app_config.mask & _CCM_BIT_LOG_LEVEL) {
            ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_WEB);
            ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_TTY);
            ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_FILE);

            g_app_config.mask &= (~_CCM_BIT_LOG_LEVEL);
        }

        memset(&yuvFrame, 0, sizeof(yuvFrame));
        yuvFrame.cbSize = sizeof(yuvFrame);

        // Get YUV channel data
        ret = ITOP_YUV_getFrame2(g_app_global.hYuv, &yuvFrame, 2000);
        if (ITOP_SUCCESS != ret)
        {
            ITOP_LOG_ERROR("Get YUV frame data fail with %#x\n", ret);
            // If the frame data cannot be obtained, pause for 100ms and try again
            usleep(100000);

            // If the number of consecutive times is less than 100, continue to try, otherwise end the task
            if (tryCnt < 100) {
                tryCnt++;
                continue;
            }
            return -1;
        }
        tryCnt = 0;

        // AI detection
        ret = app_ai_process(g_app_global.hNNX, &yuvFrame);
        if(ret != ITOP_SUCCESS)
        {
            ITOP_LOG_ERROR("get result is failure\n");
        }
		else
        {
			ITOP_LOG_INFO("get ai process result is success\n");	
		}

        // Draw OSD
        ITOP_LOG_INFO("---Draw OSD---");
        app_result_draw();
		
        
		int32_t i, w, h, leftTopX, leftTopY, rightBottomX, rightBottomY, classId;
		int32_t resultNum = g_app_global.resultNum;
		
		//遍历打印目标矩形框
		ITOP_LOG_INFO("get total %d objs \n", resultNum);
        for (i = 0; i < g_app_global.resultNum; i++) 
        {
            classId = g_app_global.aiResult[i].classId;
			leftTopX 		= g_app_global.aiResult[i].actual.lt.x;
			leftTopY 		= g_app_global.aiResult[i].actual.lt.y;
			rightBottomX 	= g_app_global.aiResult[i].actual.rb.x;
			rightBottomY 	= g_app_global.aiResult[i].actual.rb.y;
			w = rightBottomX - leftTopX;
			h = rightBottomY - leftTopY;
			ITOP_LOG_INFO("the %d obj is %s   w= %d, h= %d \t leftTop and rightBottom position is:[%d, %d] [%d, %d] \n"
			,i,g_yolo_object_list[classId], w, h, leftTopX, leftTopY, rightBottomX, rightBottomY);

			//发送帧到客户端
			app_send(&(g_app_global.aiResult[i]), &yuvFrame);
        }
		
        ret = ITOP_YUV_releaseFrame2(g_app_global.hYuv, &yuvFrame);
        if(ITOP_SUCCESS != ret) {
            ITOP_LOG_ERROR("Release YUV frame data fail with %#x\n", ret);
        }
		sleep(3);
    }

    return ITOP_FAILED;
}

// URL decoding
int app_http_urldecode(const char* src, char* outbuf, int size) {
    enum _c_t {
        normal  = 0,
        percent = 16,
        space   = 17,
    };
    int  i, k;
    char byte[128] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0, 16,  0,  0,  0,  0,  0, 17,  0,  0,  0,  0,
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
        0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    };

    for (i = 0, k = 0; src[i] != '\0'; i++) {
        if (k+1 >= size) {
            break;
        }

        switch (byte[src[i]]) {
        case percent:
            outbuf[k++] = (byte[src[i + 1]] << 4) | (byte[src[i + 2]]);
            i += 2;
            break;
        case space:
            outbuf[k++] = ' ';
            break;
        default:
            outbuf[k++] = src[i];
            break;
        }
    }
    outbuf[k++] = 0;

    return k;
}

int32_t app_http_on_request(const ITOP_HTTP_Request  *request,
                             const ITOP_HTTP_Response *response)
{
    int         ret;
    int         len;
    int         i, k;
    int         length;
    char        *cmd;
    char        *buffer;
    char        *outbuf;

    for (i = 0, k = 0; request->header->url[i] != '\0'; i++) {
        if (request->header->url[i] == '/') {
            cmd = request->header->url + i;
        }
        if (request->header->url[i] == '?') {
            break;
        }
    }

    len = 1024;
    buffer = (char*)malloc(len);

	g_app_config.selected_objs[0] = 0;
	g_app_config.selected_objs[0] = 58; //potted plant
	g_app_config.selected_objs[0] = 73; //book
	g_app_config.selected_objs[0] = 56;//chair
	
    if (0 == strncmp(cmd, "/getConfig", 10)) {
        length = 0;
        length += sprintf(buffer + length, "{\"det_rect\":[[%d,%d],[%d,%d]],\"min_rect\":[[%d,%d],[%d,%d]],",
                g_app_config.detect_region.lt_x,
                g_app_config.detect_region.lt_y,
                g_app_config.detect_region.rb_x,
                g_app_config.detect_region.rb_y,
                g_app_config.min_region.lt_x,
                g_app_config.min_region.lt_y,
                g_app_config.min_region.rb_x,
                g_app_config.min_region.rb_y
                );
        if (g_app_config.linkage_event) {
            length += sprintf(buffer + length, "\"snap\":true,");
        }
        else  {
            length += sprintf(buffer + length, "\"snap\":false,");
        }
        length += sprintf(buffer + length, "\"connect\": {\"ip\":\"%s\",\"port\":%d},", g_app_config.net_ip, g_app_config.net_port);
        length += sprintf(buffer + length, "\"logLevel\": %d,", g_app_config.log_level);
        length += sprintf(buffer + length, "\"obj_maps\":[");
        fillBuffer(buffer, &length);
        length += sprintf(buffer + length, "],\"selected\":[");
        for (i = 0; i < 16; i++) {
            length += sprintf(buffer + length, "%d,", g_app_config.selected_objs[i]);
        }
        length += sprintf(buffer + length, "0],\"status\":\"OK\"}");
        response->addHeader(request->token, "Content-Type", "application/json");
        response->setCode(request->token, ITOP_HTTP_StatusCode_200_OK);
        response->setContentLength(request->token, length);
        response->writeContent(request->token, buffer, length);
        response->writeEnd(request->token);
    }
    else if (0 == strncmp(cmd, "/setConfig", 10)) {
        memset(buffer, 0, len);

        len /= 2;
        outbuf = buffer + len;

        request->readContent((char*)request->token, buffer, (uint32_t*)&len);
        len += 1; // Add '\ 0' at the end

        app_http_urldecode(buffer, outbuf, len);

        app_config_parse(outbuf, &g_app_config);

        app_config_save(&g_app_config);

        length = sprintf(buffer, "{\"status\":\"OK\"}");

        response->addHeader(request->token, "Content-Type", "application/json");
        response->setCode(request->token, ITOP_HTTP_StatusCode_200_OK);
        response->setContentLength(request->token, length);
        response->writeContent(request->token, buffer, length);
        response->writeEnd(request->token);
    }

    return ITOP_SUCCESS;
}


void app_exit_callback()
{
    ITOP_LOG_INFO("app_size_limit-%d will exit\n", getpid());

    ITOP_HTTP_offline();

    app_net_deinit();

    app_enc_deinit();

    app_yuv_deinit();

    app_ai_deinit();

    object_ot_deinit();

    ITOP_SYS_deInit();
}

int main(int argc, char **argv)
{
    int32_t                    ret = -1;
    char                     webUrl[32] = "index.html";
    ITOP_SYS_InitParam          initParam;
    ITOP_APP_ConfigParam        appCfg;
    ITOP_HTTP_AppDefinition     webApp;

    // step1: sys init. regist exit callback func
    memset(&initParam, 0, sizeof(initParam));
    initParam.onExitCallback = app_exit_callback;
    initParam.version = ITOP_SDK_VERSION;
    ret = ITOP_SYS_init(&initParam);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_SYS_init fail with %#x\n", ret);
        return ITOP_FAILED;
    }
    ITOP_LOG_INFO("ITOP_SYS_init suscess!\n");

    /// ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    ret = app_config_init(&g_app_config);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_config_init fail with %#x\n", ret);
        goto err1;
    }
    g_app_config.mask = _CCM_BIT_NONE;
    g_app_config.log_level = ITOP_LOG_LEVEL_DEBUG;

    ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_WEB);
    ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_TTY);
    ITOP_LOG_setLevel((ITOP_LOG_Level)g_app_config.log_level, ITOP_LOG_DEST_FILE);
    // 娴嬭瘯 debug绛夌骇鏃ュ織鎵撳嵃鎴愬姛
    ITOP_LOG_DEBUG("ITOP_LOG_setLevel : set log level debug success!\n");

    object_ot_init();

    ret = app_ai_init();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_ai_init fail with %#x\n", ret);
        goto err1;
    }

    ret = app_yuv_init();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_yuv_init fail with %#x\n", ret);
        goto err2;
    }

    ret = app_enc_init();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_enc_init fail with %#x\n", ret);
        goto err3;
    }

	//创建服务端线程
	pthread_t 	serverThreadId;
	ITOP_LOG_INFO("----- start server ------\n");
	ret = pthread_create(&serverThreadId,NULL,serverStart,NULL);
	if(-1 == ret )
	{
		ITOP_LOG_INFO("pthread create fail \n");
	}
	
	
#if 0
    ret = app_net_init();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_net_init fail with %#x\n", ret);
    }
#endif

    memset(&appCfg, 0, sizeof(appCfg));
    appCfg.weburl = webUrl;
    appCfg.urllen = strlen(webUrl);
    ITOP_APP_setConfig(&appCfg);

    memset(&webApp, 0, sizeof(webApp));
    webApp.cbSize = sizeof(webApp);
    webApp.servlet = app_http_on_request;
    ret = ITOP_HTTP_online(&webApp);
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("ITOP_HTTP_online fail with %#x\n", ret);
        goto err5;
    }
	

	//开始检测目标
	ITOP_LOG_INFO("----ai task start detect!----\n");
    ret = app_ai_task();
    if (ITOP_SUCCESS != ret)
    {
        ITOP_LOG_ERROR("app_ai_task fail with %#x\n", ret);
        goto err6;
    }
    else
    {
        ITOP_LOG_INFO("app_ai_task suscess!\n");
    }

    return 0;

err6:
    ITOP_HTTP_offline();
err5:
    app_net_deinit();
err4:
    app_enc_deinit();
err3:
    app_yuv_deinit();
err2:
    app_ai_deinit();
err1:
    object_ot_deinit();
    ITOP_SYS_deInit();

    return -1;
}
