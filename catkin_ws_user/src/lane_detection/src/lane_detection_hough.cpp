#include "ros/ros.h"
#include "lane_detection.h"

cv::Mat Image;
bool image_flag=false;
cv::Mat persp;
cv::Mat transf;

void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{
        try{
                Image = cv_bridge::toCvShare(msg, "bgr8")->image;
                image_flag=true;
//     std::cout << "Received image" << std::endl;
        }catch ( cv_bridge::Exception& e)
        {
                ROS_ERROR("Could not convert from '%s' to 'bgr8'.", msg->encoding.c_str());
                return;
        }
}

int main(int argc, char** argv)
{
        ros::init(argc, argv, "image_publisher");
        ros::NodeHandle nh;
        image_transport::ImageTransport its(nh);
        image_transport::Subscriber sub = its.subscribe("image",1,imageCallback);

        image_transport::ImageTransport itp(nh);
        image_transport::Publisher pub = itp.advertise("lines",5);

        ros::Publisher angle_pub_r = nh.advertise<std_msgs::Float32MultiArray>("/rightLine",100);
        ros::Publisher angle_pub_l = nh.advertise<std_msgs::Float32MultiArray>("/leftLine",100);

        ros::Rate loop_rate(60);

        int hough_thr;
        double min_lin_len,max_gap_len;
        int value_thr_low, value_thr_high;
        int canny_thr_low, canny_thr_high;

        nh.param<int>("value_thr_low",value_thr_low,170);
        nh.param<int>("value_thr_high",value_thr_high,190);

        nh.param<int>("hough_thr",hough_thr,50);
        nh.param<double>("min_lin_len",min_lin_len,30);
        nh.param<double>("max_gap_len",max_gap_len,10);

        nh.param<int>("canny_thr_low",canny_thr_low,50);
        nh.param<int>("canny_thr_high",canny_thr_high,100);

        lane_extractor extractor(hough_thr,min_lin_len,max_gap_len,value_thr_low,
                                 value_thr_high,canny_thr_low,canny_thr_high);
        std::string filepath;
        nh.param<std::string>("calib_file",filepath,"Matrix2.yaml"); //Set file path_pub

        cv::Mat transfMatrix;


        cv::FileStorage fs(filepath, cv::FileStorage::READ);
        if (!fs.isOpened())
        {
                std::cout<<"No Mat @"<<filepath<<std::endl;
                return 0;
        }else
        {
                std::cout<<"***********Mat FOUND @ "<<filepath<<"***********"<<std::endl;
        }
        fs["Homography"] >> transfMatrix;
        cv::Size transfSize;
        fs["tSize"] >> transfSize;
        fs.release();

        std::cout<<"Starting processing"<<std::endl;


        bool color=false;
        if (argc>1)
        {
                if(std::string(argv[1])=="color")
                {
                        color=true;
                }
                std::cout << "Processing: " << argv[1]<< " "<< color <<'\n';

        }
        else
        {
                std::cout << "Usage lane_hough_node color||bw" << '\n';
        }



        while (ros::ok()) {
                if (image_flag)
                {
                        cv::Rect rect(0,240,640,240); //lower half of image
                        cv::Mat resizeImage= Image(rect);

                        //ros::Time strt_time = ros::Time::now();
                        image_flag = false;
                        std_msgs::Float32MultiArray line_r,line_l;
                        cv::Mat trans,edges,viz;
                        cv::warpPerspective(resizeImage, trans, transfMatrix, transfSize, cv::INTER_LINEAR, cv::BORDER_REPLICATE, cv::Scalar(127, 127, 127) );
                        extractor.get_borders(trans,edges,color);
                        cv::cvtColor(edges,viz,cv::COLOR_GRAY2BGR);
                        line_r=extractor.extract_right_lane_hough(edges,viz);
                        line_l=extractor.extract_left_lane_hough(edges,viz);
                        cv::addWeighted(viz, 0.5, trans, 0.5, 0, viz);
                        sensor_msgs::ImagePtr msg=cv_bridge::CvImage(std_msgs::Header(),"bgr8",viz).toImageMsg();

                        pub.publish(msg);
                        if(line_r.data.size()>0)
                        {
                                angle_pub_r.publish(line_r);
                        }
                        if(line_l.data.size()>0)
                        {
                                angle_pub_l.publish(line_l);
                        }

                        // ros::Duration timing=ros::Time::now()-strt_time;
                        // std::cout << timing.toSec() << '\n';
                }
                ros::spinOnce();
                loop_rate.sleep();
        }
        return 0;
}
