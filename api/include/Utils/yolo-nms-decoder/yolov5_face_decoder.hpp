#ifndef _YOLO_FACE_DECODER_H_
#define _YOLO_FACE_DECODER_H_

#include <vector>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <functional>
#include <type_traits>

struct FaceDetectionObject {
    float ymin, xmin, ymax, xmax, confidence;
    float landmark1_x, landmark1_y, landmark2_x, landmark2_y, landmark3_x, landmark3_y, landmark4_x, landmark4_y, landmark5_x, landmark5_y;
    int class_id;

    FaceDetectionObject(float ymin, float xmin, float ymax, float xmax, float confidence, int class_id,
                        float landmark1_x, float landmark1_y, float landmark2_x, float landmark2_y, float landmark3_x, float landmark3_y,
                        float landmark4_x, float landmark4_y, float landmark5_x, float landmark5_y):
        ymin(ymin), xmin(xmin), ymax(ymax), xmax(xmax), confidence(confidence), class_id(class_id),
        landmark1_x(landmark1_x), landmark1_y(landmark1_y), landmark2_x(landmark2_x), landmark2_y(landmark2_y),
        landmark3_x(landmark3_x), landmark3_y(landmark3_y), landmark4_x(landmark4_x), landmark4_y(landmark4_y),
        landmark5_x(landmark5_x), landmark5_y(landmark5_y)
        {}
    bool operator<(const FaceDetectionObject &s2) const {
        return this->confidence > s2.confidence;
    }
};

struct YoloFaceDecodedBox {
    float x;
    float y;
    float w;
    float h;
};

struct YoloFaceQunatizationInfo {
    float qp_zp;
    float qp_scale;
};

struct YoloFaceOutputInfo {

    int FeatureSizeRow;
    int FeatureSizeCol; 
    std::vector<int> anchors;    
    YoloFaceQunatizationInfo qInfo;
};


template <class T>
class YoloFaceDecoder {

    const float IOU_THRESHOLD = 0.45f;
    const int MAX_BOXES = 200;
    const int CONF_CHANNEL_OFFSET = 4;
    const int CLASS_CHANNEL_OFFSET = 15;
    int ImageSizeWidth = 640;
    int ImageSizeHeight = 640;
    int YoloAnchorNum = 0;
    int TotalOutputAdded = 0;
    int TotalClass = 1;
    int TotalFeatureMapChannel = TotalClass+15;
    float ConfidenceThreshold = 0.3f;
    std::vector<YoloFaceOutputInfo> OutputInfoList;

    float sigmoid(float x) {
        //returns the value of the sigmoid function f(x) = 1/(1 + e^-x)
        return 1.0f / (1.0f + exp(-x));
    }

    float iou_calc(const FaceDetectionObject &box_1, const FaceDetectionObject &box_2) {
        const float width_of_overlap_area = std::min(box_1.xmax, box_2.xmax) - std::max(box_1.xmin, box_2.xmin);
        const float height_of_overlap_area = std::min(box_1.ymax, box_2.ymax) - std::max(box_1.ymin, box_2.ymin);
        const float positive_width_of_overlap_area = std::max(width_of_overlap_area, 0.0f);
        const float positive_height_of_overlap_area = std::max(height_of_overlap_area, 0.0f);
        const float area_of_overlap = positive_width_of_overlap_area * positive_height_of_overlap_area;
        const float box_1_area = (box_1.ymax - box_1.ymin)  * (box_1.xmax - box_1.xmin);
        const float box_2_area = (box_2.ymax - box_2.ymin)  * (box_2.xmax - box_2.xmin);
        return area_of_overlap / (box_1_area + box_2_area - area_of_overlap);
    }

    void extract_boxes( T* fm, YoloFaceQunatizationInfo &q_info, int feature_map_size_row, int feature_map_size_col, 
                        std::vector<int> &Anchors, std::vector<FaceDetectionObject> &objects, float& thr) {

        float  confidence, x, y, h, w, xmin, ymin, xmax, ymax, landm1_x, landm1_y, landm2_x, landm2_y, landm3_x, landm3_y, landm4_x, landm4_y, landm5_x, landm5_y, conf_max = 1.0f;
        int add = 0, chosen_cls = -1, row_ind = 0, feature_map_stride = 0;

        //PreCalculation for optimization
        int lcYoloAnchorNum = this->YoloAnchorNum;

        //Temp for now
        feature_map_stride = this->ImageSizeWidth / feature_map_size_row;

        for (int row = 0; row < feature_map_size_row; ++row) {
            for (int col = 0; col < feature_map_size_col; ++col) {
                for (int a = 0; a < lcYoloAnchorNum; ++a) {

                    add = row_ind * 16;
                    row_ind++;
                    confidence = sigmoid(this->dequantize(fm[add+4], q_info));

                    //if (confidence < thr)
                    //    continue;

                    float face_score = sigmoid(this->dequantize(fm[add+15], q_info));
                    conf_max = face_score * confidence;

                    if (conf_max >= thr) {

                        //box_xy = (y[:, :, :, :, 0:2] * 2. - 0.5 + self.grid[i].to(x[i].device)) * self.stride[i]
                        x = (sigmoid(this->dequantize(float(fm[add]), q_info)) * 2.0f - 0.5f + (float)col) * (float)feature_map_stride;
                        y = (sigmoid(this->dequantize(float(fm[add+1]), q_info)) * 2.0f - 0.5f +  (float)row) * (float)feature_map_stride;
                        // box_wh = (y[:, :, :, :, 2:4] * 2) ** 2 * self.anchor_grid[i]
                        w = pow(2.0f * (sigmoid(this->dequantize(float(fm[add+2]), q_info))), 2.0f) * (float)Anchors[a * 2];
                        h = pow(2.0f * (sigmoid(this->dequantize(float(fm[add+3]), q_info))), 2.0f) * (float)Anchors[a * 2 + 1];

                        //landm1 = y[:, :, :, :, 5:7] * self.anchor_grid[i] + self.grid[i].to(x[i].device) * self.stride[i]
                        landm1_x = (this->dequantize(float(fm[add+5]), q_info) * (float)Anchors[a * 2] + (float)col * (float)feature_map_stride);
                        landm1_y = (this->dequantize(float(fm[add+6]), q_info) * (float)Anchors[a * 2 + 1] + (float)row * (float)feature_map_stride);

                        landm2_x = (this->dequantize(float(fm[add+7]), q_info) * (float)Anchors[a * 2] + (float)col * (float)feature_map_stride);
                        landm2_y = (this->dequantize(float(fm[add+8]), q_info) * (float)Anchors[a * 2 + 1] + (float)row * (float)feature_map_stride);

                        landm3_x = (this->dequantize(float(fm[add+9]), q_info) * (float)Anchors[a * 2] + (float)col * (float)feature_map_stride);
                        landm3_y = (this->dequantize(float(fm[add+10]), q_info) * (float)Anchors[a * 2 + 1] + (float)row * (float)feature_map_stride);

                        landm4_x = (this->dequantize(float(fm[add+11]), q_info) * (float)Anchors[a * 2] + (float)col * (float)feature_map_stride);
                        landm4_y = (this->dequantize(float(fm[add+12]), q_info) * (float)Anchors[a * 2 + 1] + (float)row * (float)feature_map_stride);

                        landm5_x = (this->dequantize(float(fm[add+13]), q_info) * (float)Anchors[a * 2] + (float)col * (float)feature_map_stride);
                        landm5_y = (this->dequantize(float(fm[add+14]), q_info) * (float)Anchors[a * 2 + 1] + (float)row * (float)feature_map_stride);

                        // 过滤非法坐标
                        xmin = std::max((x - (w / 2.0f)), 0.0f);
                        ymin = std::max((y - (h / 2.0f)), 0.0f);
                        xmax = std::min((x + (w / 2.0f)), (static_cast<float>(640) - 1));
                        ymax = std::min((y + (h / 2.0f)), (static_cast<float>(640) - 1));
                        w = xmax - xmin;
                        h = ymax - ymin;

                        if((w <= 0) or (h <= 0)){
                            std::cout << "dropped" << std::endl;
                            continue;
                        }

                        chosen_cls = 1;
                        objects.push_back(FaceDetectionObject(  ymin, xmin, ymax, xmax, conf_max, chosen_cls,
                                                                landm1_x,landm1_y,landm2_x,landm2_y,landm3_x,landm3_y,
                                                                landm4_x,landm4_y,landm5_x,landm5_y));
                    }                   
                }
            }
        }
    }    

public:

    virtual float      output_data_trasnform(float data) {
        return data;
    }

    virtual YoloFaceDecodedBox box_decode(  T* fm, int fm_offset, 
                                    std::vector<int> &Anchors, int anchor_index, YoloFaceQunatizationInfo &q_info,
                                    int feature_map_size_row, int feature_map_size_col, int chosen_col, int chosen_row) = 0;

    float dequantize(float input, YoloFaceQunatizationInfo &q_info) {
        return (input - q_info.qp_zp) * q_info.qp_scale;

    }

    int  getImageWidth() {
        return this->ImageSizeWidth;
    }

    int  getImageHeight() {
        return this->ImageSizeHeight;
    }

    void YoloConfig (int ImageWidth, int ImageHeight, int TotalClass, float ConfidenceThreshold, int totalAnchorNum = 3) {
        this->ImageSizeWidth = ImageWidth;
        this->ImageSizeHeight = ImageHeight;
        this->TotalClass = TotalClass;     
        this->TotalFeatureMapChannel = this->TotalClass + 15;
        this->ConfidenceThreshold = ConfidenceThreshold;
        this->YoloAnchorNum = totalAnchorNum;
    }

    int YoloAddOutput (int FeatureSizeRow, int FeatureSizeCol, std::vector<int> anchors, YoloFaceQunatizationInfo* pQInfo = nullptr) {

        YoloFaceOutputInfo OutputInfo;
        OutputInfo.FeatureSizeRow = FeatureSizeRow;
        OutputInfo.FeatureSizeCol = FeatureSizeCol;
        OutputInfo.anchors = anchors;

        if (pQInfo == nullptr) {
            OutputInfo.qInfo.qp_scale = 1.0f;
            OutputInfo.qInfo.qp_zp = 0.0f;
            if ((typeid(uint8_t).name() == typeid(T).name()))
                std::cout << "WARNING: Raw data declaration detected while quantization info is not proivded (for dequantization from raw int to float)" << std::endl;
        }
        else {
            OutputInfo.qInfo.qp_scale = pQInfo->qp_scale;
            OutputInfo.qInfo.qp_zp = pQInfo->qp_zp;
        }

        this->OutputInfoList.push_back(OutputInfo);
        this->TotalOutputAdded++;

        return this->TotalOutputAdded;
    }

    std::vector<float> decode(std::vector<T> &Output1, std::vector<T> &Output2, std::vector<T> &Output3) {

        return decode(  (Output1.size() > 0 ) ? Output1.data() : NULL,
                        (Output2.size() > 0 ) ? Output2.data() : NULL,
                        (Output2.size() > 0 ) ? Output2.data() : NULL
                     );

    }

    std::vector<float> decode(T* Output1, T* Output2, T* Output3) {
        size_t num_boxes = 0;
        std::vector<FaceDetectionObject> objects;
        objects.reserve(MAX_BOXES);
        
        if (Output1 != NULL) {
            this->extract_boxes(Output1, this->OutputInfoList[0].qInfo, this->OutputInfoList[0].FeatureSizeRow, this->OutputInfoList[0].FeatureSizeCol, 
                                this->OutputInfoList[0].anchors, objects, this->ConfidenceThreshold);
        }

        if (Output2 != NULL) {
            this->extract_boxes(Output2, this->OutputInfoList[1].qInfo, this->OutputInfoList[1].FeatureSizeRow, this->OutputInfoList[1].FeatureSizeCol, 
                                this->OutputInfoList[1].anchors, objects, this->ConfidenceThreshold);
        }

        if (Output3 != NULL) {
            this->extract_boxes(Output3, this->OutputInfoList[2].qInfo, this->OutputInfoList[2].FeatureSizeRow, this->OutputInfoList[2].FeatureSizeCol, 
                                this->OutputInfoList[2].anchors, objects, this->ConfidenceThreshold);
        }

        
#if 1
        // filter by overlapping boxes
        num_boxes = objects.size();
        std::cout << "num boxes: " << num_boxes << std::endl;
        if (objects.size() > 0) {
            std::sort(objects.begin(), objects.end(), [&](FaceDetectionObject b1, FaceDetectionObject b2){return b1.confidence > b2.confidence;});
            //std::sort(objects.begin(), objects.end());
            
            for (unsigned int i = 0; i < objects.size(); ++i) {
                
                //std::cout << objects[i].confidence << std::endl;

                if (objects[i].confidence <= this->ConfidenceThreshold) {
                    objects[i].confidence = 0;
                    //num_boxes -= 1;
                    continue;
                }

                for (unsigned int j = i + 1; j < objects.size(); ++j) {
                    if (objects[i].class_id == objects[j].class_id && objects[j].confidence >= this->ConfidenceThreshold) {
                        if (iou_calc(objects[i], objects[j]) >= IOU_THRESHOLD) {
                            
                            objects[j].confidence = 0;
                            num_boxes -= 1;
                        }
                    }
                }
                
            }
            
        }

        std::cout << "num boxes: " << num_boxes << std::endl;
        
        // Copy the results
        std::vector<float> results;
        if (num_boxes > 0) {
            int box_ptr = 0;

            results.resize(num_boxes * 16);
            for (const auto &obj: objects) {
                if (obj.confidence >= this->ConfidenceThreshold) {
                    results[box_ptr*16 + 0] = obj.ymin / 640.0f;
                    results[box_ptr*16 + 1] = obj.xmin / 640.0f;
                    results[box_ptr*16 + 2] = obj.ymax / 640.0f;
                    results[box_ptr*16 + 3] = obj.xmax / 640.0f;
                    results[box_ptr*16 + 4] = (float)obj.class_id;
                    results[box_ptr*16 + 5] = obj.confidence;
                    
                    results[box_ptr*16 + 6] = obj.landmark1_x;
                    results[box_ptr*16 + 7] = obj.landmark1_y;
                    results[box_ptr*16 + 8] = obj.landmark2_x;
                    results[box_ptr*16 + 9] = obj.landmark2_y;
                    results[box_ptr*16 + 10] = obj.landmark3_x;
                    results[box_ptr*16 + 11] = obj.landmark3_y;
                    results[box_ptr*16 + 12] = obj.landmark4_x;
                    results[box_ptr*16 + 13] = obj.landmark4_y;
                    results[box_ptr*16 + 14] = obj.landmark5_x;
                    results[box_ptr*16 + 15] = obj.landmark5_y;

                    box_ptr += 1;          
                    //std::cout << "ymin: " << obj.ymin << ", xmin: " << obj.xmin << ", ymax: " << obj.ymax << ", xmax: " << obj.xmax << ", id: " << obj.class_id << ", conf: " << obj.confidence <<std::endl;          
                }
            }
            return results;
        } else {        
            results.resize(0);
            return results;
        }
#else
        int index_pos = 6;
        std::vector<float> results;
        results.resize(6);
        results[0] = objects[index_pos].ymin / 640.0f;
        results[1] = objects[index_pos].xmin / 640.0f;
        results[2] = objects[index_pos].ymax / 640.0f;
        results[3] = objects[index_pos].xmax / 640.0f;
        results[4] = (float)objects[index_pos].class_id;
        results[5] = objects[index_pos].confidence; 
        return results;
#endif

    }
};



template <class T>
class Yolov5FaceDecoder : public YoloFaceDecoder <T> {

    bool        bActivationIsSigmoid = false;
    
    float sigmoid(float x) {
        // returns the value of the sigmoid function f(x) = 1/(1 + e^-x)
        return 1.0f / (1.0f + exp(-x));
    }

    //Override parent
    float output_data_trasnform(float data) {

        if (this->bActivationIsSigmoid)
            return data;
        
        return sigmoid(data);  

    }
    
public:

    Yolov5FaceDecoder(bool bSigmoidActivation) {
        this->bActivationIsSigmoid = bSigmoidActivation;
    }

    //Override parent
    YoloFaceDecodedBox box_decode(  T* fm, int fm_offset, 
                            std::vector<int> &Anchors, int anchor_index, YoloFaceQunatizationInfo &q_info,
                            int feature_map_size_row, int feature_map_size_col, int chosen_col, int chosen_row) {
        
        YoloFaceDecodedBox DBox;

        //All done in the parent class

        return DBox;
    }

};


#endif
