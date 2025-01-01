import cv2
import os
import numpy as np
from datetime import datetime
import subprocess


def get_image_files(folder_path):
    """ 获取指定文件夹下的所有 .bmp 文件，并按文件名排序 """
    files = [f for f in os.listdir(folder_path) if f.endswith('.bmp')]
    files.sort()  # 按文件名排序
    return [os.path.join(folder_path, f) for f in files]


def create_video_from_images(image_folder, output_video, fps=30):
    # 获取文件夹中的所有图片文件
    image_files = get_image_files(image_folder)
    if not image_files:
        print("No images found in the folder.")
        return

    # 读取第一张图片以获取帧大小
    first_frame = cv2.imread(image_files[0])
    height, width, _ = first_frame.shape

    # 初始化视频写入器
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')  # 使用 mp4v 编码器
    video_writer = cv2.VideoWriter(output_video, fourcc, fps, (width, height))

    # 遍历所有图片文件并写入视频
    for file in image_files:
        frame = cv2.imread(file)
        if frame is None:
            print(f"Failed to read image: {file}")
            continue
        # 如果图片大小与第一张图片不一致，调整大小
        if frame.shape[0] != height or frame.shape[1] != width:
            new_frame = np.zeros((height, width, 3), np.uint8)
            new_frame[:frame.shape[0], :frame.shape[1]] = frame
            frame = new_frame
        video_writer.write(frame)
        print(f"Added {file} to video.")

    # 释放视频写入器
    video_writer.release()
    print(f"Video saved as {output_video}")


def compress_video_ffmpeg(temp_video_path, output_video_path, bitrate="2M"):
    # 使用 FFmpeg 对视频进行压缩
    command = [
        "ffmpeg", "-i", temp_video_path,
        "-b:v", bitrate,        # 设置比特率
        "-c:v", "libx264",      # 使用 h.264 编码器
        "-preset", "fast",      # 设置压缩速度
        "-movflags", "+faststart",
        output_video_path
    ]
    subprocess.run(command)
    print(f"Compressed video saved as {output_video_path}")


# 配置参数
image_folder = r"D:\screenCap"  # 替换为你的图片文件夹路径
output_video = "output_video.mp4"  # 输出视频文件名
fps = 60  # 视频帧率

# 生成视频
create_video_from_images(image_folder, output_video, fps)

# 压缩视频
compress_video_ffmpeg(output_video, "compressed_video.mp4", bitrate="8M")

# 删除原始视频
os.remove(output_video)
