import os
import paramiko

REMOTE_HOST = "chengyongkang.me"
REMOTE_USER = "ken"
KEY_FILE = r"C:\Users\IWMAI\.ssh\id_rsa"  # 私钥文件路径
REMOTE_DIR = "/home/ken/Documents/cameraCap"  # 远程要下载的根目录

LOCAL_DIR = r"D:\cameraCap"  # 本地保存根目录

def get_local_files(base_path):
    """
    获取本地已有文件的(相对路径)集合，用于避免重复下载。
    例如: {"2025-01-01/08-30-00.jpg", "2025-01-01/09-00-00.jpg", ...}
    """
    local_files = set()
    for root, dirs, files in os.walk(base_path):
        for f in files:
            # 构造相对路径
            abs_path = os.path.join(root, f)
            rel_path = os.path.relpath(abs_path, base_path)  # 相对于 base_path 的子路径
            local_files.add(rel_path.replace("\\", "/"))  # 统一用 '/' 作为分隔符
    return local_files

def sftp_walk(sftp, remote_path):
    """
    类似 os.walk，用于递归获取远程服务器上的目录结构。
    产出 (当前路径, 子目录列表, 文件列表)，当前路径是相对于最初 remote_path 的一个子路径。
    """
    # stack 用于模拟 DFS
    stack = [(remote_path, "")]
    while stack:
        fullpath, relpath = stack.pop()
        try:
            items = sftp.listdir_attr(fullpath)
        except IOError:
            # 如果远程不是目录或无法访问
            continue

        dirs = []
        files = []
        for item in items:
            remote_name = item.filename
            remote_path_item = os.path.join(fullpath, remote_name).replace("\\", "/")
            if stat_is_dir(item):  # 是目录
                dirs.append(remote_name)
                new_relpath = os.path.join(relpath, remote_name).replace("\\", "/")
                stack.append((remote_path_item, new_relpath))
            else:
                files.append(remote_name)

        yield relpath, dirs, files

def stat_is_dir(sftp_attr):
    """
    判断 sftp.listdir_attr() 返回的对象是否为目录
    """
    # 在 posix 下用 st_mode 判断是否为目录
    from stat import S_ISDIR
    return S_ISDIR(sftp_attr.st_mode)

def main():
    # 1. 获取本地已有文件清单
    if not os.path.exists(LOCAL_DIR):
        os.makedirs(LOCAL_DIR)
    local_files = get_local_files(LOCAL_DIR)
    
    # 2. 建立 SSH 连接 & SFTP
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    try:
        ssh.connect(
            hostname=REMOTE_HOST,
            username=REMOTE_USER,
            key_filename=KEY_FILE
        )
        sftp = ssh.open_sftp()

        # 3. 遍历远程目录
        for relpath, dirs, files in sftp_walk(sftp, REMOTE_DIR):
            # 确保本地对应的目录存在
            local_subdir = os.path.join(LOCAL_DIR, relpath)
            if not os.path.exists(local_subdir):
                os.makedirs(local_subdir)

            # 遍历文件
            for f in files:
                # 构造远程文件和本地文件的完整路径
                remote_file = os.path.join(REMOTE_DIR, relpath, f).replace("\\", "/")
                local_file_rel = os.path.join(relpath, f).replace("\\", "/") 
                if local_file_rel in local_files:
                    # 已经在本地存在，则跳过
                    print(f"Skipping (already exists): {local_file_rel}")
                    continue
                else:
                    # 需要下载
                    local_file = os.path.join(LOCAL_DIR, local_file_rel)
                    print(f"Downloading: {remote_file} -> {local_file}")
                    sftp.get(remote_file, local_file)
                    # 记得把新下的文件加到 local_files 里
                    local_files.add(local_file_rel)

        sftp.close()
        ssh.close()
        print("Download complete.")
    except Exception as e:
        print("Error occurred:", e)

if __name__ == "__main__":
    main()
