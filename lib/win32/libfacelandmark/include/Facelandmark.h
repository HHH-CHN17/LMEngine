#ifndef FACELANDMARK_H
#define FACELANDMARK_H

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <string>
#include "ofvec2f.h"

extern "C"{

bool FACETRACKER_API_init_facetracker_resources(char* filePath);

void FACETRACKER_API_release_all_resources();

void FACETRACKER_API_facetracker_obj_reset();

bool FACETRACKER_API_facetracker_obj_track(cv::Mat &captureImage );

ofVec2f FACETRACKER_API_getPosition(cv::Mat &currentShape);

float FACETRACKER_API_getDistance(ofVec2f p1, ofVec2f p2);

float FACETRACKER_API_getScale(cv::Mat &currentShape);

}


#endif // FACELANDMARK_H
