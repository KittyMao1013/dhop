#include "yolo_object.h"
#include <stdio.h>

const char *g_yolo_object_list[80] =
{
    "person",//00
    "bicycle",
    "car",
    "motorcycle",
    "airplane",
    "bus",
    "train",
    "truck",
    "boat",
    "traffic light",
    "fire hydrant",//10
    "stop sign",
    "parking meter",
    "bench",
    "bird",
    "cat",
    "dog",
    "horse",
    "sheep",
    "cow",
    "elephant",//20
    "bear",
    "zebra",
    "giraffe",
    "backpack",
    "umbrella",
    "handbag",
    "tie",
    "suitcase",
    "frisbee",
    "skis",//30
    "snowboard",
    "sports ball",
    "kite",
    "baseball bat",
    "baseball glove",
    "skateboard",
    "surfboard",
    "tennis racket",
    "bottle",
    "wine glass",//40
    "cup",
    "fork",
    "knife",
    "spoon",
    "bowl",
    "banana",
    "apple",
    "sandwich",
    "orange",
    "broccoli",//50
    "carrot",
    "hot dog",
    "pizza",
    "donut",
    "cake",
    "chair",
    "couch",
    "potted plant",
    "bed",
    "dining table",//60
    "toilet",
    "tv",
    "laptop",
    "mouse",
    "remote",
    "keyboard",
    "cell phone",
    "microwave",
    "oven",
    "toaster",//70
    "sink",
    "refrigerator",
    "book",
    "clock",
    "vase",
    "scissors",
    "teddy bear",
    "hair drier",
    "toothbrush"//79
};

char g_modelfile[] = "./model/sample.nnx";

void fillBuffer(char* buffer, int* len)
{
    int length = *len;
    length += sprintf(buffer + length, "{\"id\":39, \"name\":\"Bottle\"},");
    length += sprintf(buffer + length, "{\"id\":73, \"name\":\"Book\"},");
    length += sprintf(buffer + length, "{\"id\":2, \"name\":\"Car\"},");
    length += sprintf(buffer + length, "{\"id\":15, \"name\":\"Cat\"},");
    length += sprintf(buffer + length, "{\"id\":56, \"name\":\"Chair\"},");
    length += sprintf(buffer + length, "{\"id\":74, \"name\":\"Clock\"},");
    length += sprintf(buffer + length, "{\"id\":41, \"name\":\"Cup\"},");
    length += sprintf(buffer + length, "{\"id\":16, \"name\":\"Dog\"},");
    length += sprintf(buffer + length, "{\"id\":66, \"name\":\"Keyboard\"},");
    length += sprintf(buffer + length, "{\"id\":0, \"name\":\"People\"},");
    length += sprintf(buffer + length, "{\"id\":62, \"name\":\"TV\"}");
    *len = length;
}