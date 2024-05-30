# H15 Hailo Media Library C++ Inference Application Example 

The purpose of this inference application example is to provide a full framework pipeline development in C++ on top of Hailo Media Library, this a allows user to be able to use the same framework to develop their own application using different network.  

## Key Feature
* Running a code using C++ given lightweighted pipeline infra structure/framework.
* Providing ready to use modules such as resize, inference, drawing, and streaming.
* Yolov5 object detection with post-processing in C++.


## Application Pipeline
![image](/api/examples/native_custom/Resources/Image/ApplicationPipeline.png)


## Infrastructure

### Core

* The infrastructure/framework uses <code> pipeline.hpp </code>and <code> stages.hpp </code> to assemble the entire application.
* You would create a pipeline, add your stages, and subscribe the buffer that pass from stages to stages.
* Stages is the module implementation in the pipeline, which you would normally subclass it for your own implementation.
  * You add all your stages/modules to the pipeline
  * You will subcribe specific stage to a desired stage in order to pass the result (data buffer) from a stage to another.
    * In this code as example <code> frontend_aggregator_stage->add_subscriber(detector_resize_stage); </code>
      * frontend_aggregator_stage and detector_resize_stage is both a stage/module.
      * Here we are subscribing detector_resize_stage TO frontend_aggregator_stage.
      * Meaning that whenever frontend_aggregator_stage has processed data that it sent to its subcriber detector_resize_stage will received it.
    * Note that you can add multiple <code> add_subscriber </code>, the stages will send data to all of its subscriber.
* For detail please go over the code directly based on below diagram where it provides the pipeline flow with relative file/class.


![image](/api/examples/native_custom/Resources/Image/InfrastructurePipeline.png)


### Data Composition

* The data of each stages is send to each of its subscriber as mentioned earlier.
* In the reference example, we use <code> BufferPtr </code> to keep the data buffer types <code> HailoMediaLibraryBufferPtr </code> and meta datas <code> BufferMetadataPtr </code>
* Should developer decide to use the same data composition he/she can modify the code to add different data buffer types as well as metadata to fit specific project needs. Please refer to the following file for details
  * <code> infra/base.hpp </code>
  * <code> infra/metadata.hpp </code>
* NOTE: Its is not suggested to remove metadata and/or buffer type since you can send the data buffer to multiple suscribers. Removing it on one subscriber can lead to an issue while other subscriber is still using it. It is highly recommended to simply have all buffer type and metadata propagate through the entire pipeline since its not a copy but just a pointer reference which will automatically get destroyed once its not being used.


![image](/api/examples/native_custom/Resources/Image/DataComposition.png)


NOTE: The provided infrastructure is not part of sdk of H15, this is just a reference code for developer to get started and demonstrate the entire workflow on how to use H15 APIs. Developer can make any changes to the code and data composition to fit to project specific needs or use any other infrastructure/framework since its no a part of the SDK & API to obtain the frame, resize, infer, and encode.


## Prerequisites: <br />
In order to compile this application you'll need the H-15 cross-development toolchain.
The toolchain can be installed by running the script in the `sdk` folder which is a part of the Vision Processor Software Package.<br/>
In order to run this application you'll need a Hailo-15 platform connected with ethernet to your development machine.
For more information please check the EVB/SBC Quick Start Guide.
<br/>

## Running this example (Tested on Release 1.3.1)
* On your development machine
  * After installing the sdk and sourced into the sdk enviroment (refer to hailo media library document for detail instruction)
  * Compile the example for Hailo-15
    * <code> meson build </code>
    * <code> ninja -C build </code>
  * After build is complete you can deploy everything on H15 by <code> ./DeployAll.sh </code>
    * NOTE: Make sure you have connected to H15 and able to ssh into it before running DeployAll.sh
* On your Hailo-15 platform
  * Go to <code> custom_full_inference_app folder </code>
  * Execute example <code> ./custom_full_inference_example </code>
* On any PC that connects to H15
  * You can see the streaming via rtsp in 2 resolution: 4K and 720p
    * 4K <code> rtsp://10.0.0.1:5000/H15RtspStream_sink0 </code>
    * 720p <code> vlc rtsp://10.0.0.1:5002/H15RtspStream_sink1 </code>
