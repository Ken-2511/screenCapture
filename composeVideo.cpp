#include <iostream>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std;
using namespace cv;

int main() {
    string folderPath = "path/to/your/folder";  // 替换为图片文件夹路径
    string outputVideoFile = "output_video.avi"; // 输出视频文件名
    int fps = 30;  // 视频帧率

    // 1. 获取文件夹中的所有图片文件
    vector<string> imageFiles;
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bmp") {
            imageFiles.push_back(entry.path().string());
        }
    }
    if (imageFiles.empty()) {
        cerr << "No images found in the folder." << endl;
        return -1;
    }

    // 2. 对文件名进行排序，确保图片按时间顺序排列
    sort(imageFiles.begin(), imageFiles.end());

    // 3. 获取第一张图片的尺寸以确定视频帧大小
    Mat firstFrame = imread(imageFiles[0]);
    if (firstFrame.empty()) {
        cerr << "Failed to read the first image." << endl;
        return -1;
    }
    Size frameSize(firstFrame.cols, firstFrame.rows);

    // 4. 初始化视频写入器
    VideoWriter videoWriter(outputVideoFile, VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, frameSize);
    if (!videoWriter.isOpened()) {
        cerr << "Could not open the video writer." << endl;
        return -1;
    }

    // 5. 读取每张图片并写入视频
    for (const string& file : imageFiles) {
        Mat frame = imread(file);
        if (frame.empty()) {
            cerr << "Failed to read image: " << file << endl;
            continue;
        }
        // 确保图片大小匹配帧大小
        if (frame.size() != frameSize) {
            resize(frame, frame, frameSize);
        }
        videoWriter.write(frame);  // 将图片写入视频
        cout << "Added " << file << " to video." << endl;
    }

    // 6. 释放资源
    videoWriter.release();
    cout << "Video saved as " << outputVideoFile << endl;

    return 0;
}
