# jar-console-bridge

`jar-console-bridge` 面向只能通过固定命令启动进程的游戏面板。它把面板的
stdin、stdout、stderr 连接到交互式 shell，同时让 `server__autoLaunch.sh`
在后台运行。

项目现在提供两种交付形态，二者行为一致：

| 形态 | 适用场景 | 构建产物 | 运行时要求 |
| --- | --- | --- | --- |
| Java jar | 面板只允许 `java -jar server.jar` | `target/server.jar` | Java 8 或更高版本 |
| 原生二进制 | 面板允许直接启动可执行文件 | `build/native/server` | Linux x86_64，无 Java 运行时依赖 |

两种形态使用完全相同的 `server__autoLaunch.sh` 和 `server__path.sh`。
Java 版由 Maven 嵌入资源，原生版由 CMake 在编译时嵌入同一份资源，避免维护
两套启动配置。

## 功能

- 将面板控制台连接到交互式 Bash 或 sh
- 在后台运行 `server__autoLaunch.sh`
- 使用 `server__path.sh` 统一配置交互式 shell 和后台脚本的 PATH
- 首次运行时从程序内释放启动脚本到可执行文件或 jar 所在目录
- 优先使用 Bash，不存在时回退到 sh
- 前台 shell 退出后清理后台启动进程组
- 使用构建产物所在目录作为工作目录，不依赖外部当前目录

## 目录结构

```text
src/main/java/ShimMain.java       Java 启动器
src/main/resources/*.sh           两种版本共享的 shell 资源
native/CMakeLists.txt             原生版构建定义
native/src/main.cpp               C++17 原生启动器
native/src/embedded_resources.hpp.in
                                  资源嵌入模板
```

## 构建 Java 版

Windows：

```powershell
.\mvnw.cmd clean package
```

Linux 或 macOS：

```bash
./mvnw clean package
```

产物：

```text
target/server.jar
```

启动方式：

```bash
java -jar server.jar
```

## 构建原生版

原生版当前目标为 POSIX/Linux x86_64，需要 CMake 3.16+ 和支持 C++17 的编译器。
推荐在 Linux 环境构建；CI 固定使用 Ubuntu 22.04 生成全静态发布二进制，
目标机不需要预先安装 glibc 或 C++ 运行库。

```bash
cmake -S native -B build/native \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHIM_FULLY_STATIC=ON \
  -DSHIM_WARNINGS_AS_ERRORS=ON
cmake --build build/native --parallel
```

产物：

```text
build/native/server
```

启动方式：

```bash
chmod +x server
./server
```

## 运行时脚本

首次启动时，启动器会在自身所在目录写入：

```text
server__autoLaunch.sh
server__path.sh
```

如果这两个文件已经存在，启动器会保留现有内容。因此：

- 临时调整启动命令或 PATH 时，可以直接修改运行目录中的脚本。
- 需要随发行版本携带新脚本时，应修改 `src/main/resources/` 后重新构建。
- `server__path.sh` 只用于 PATH 配置；服务启动命令应放在
  `server__autoLaunch.sh`。

## CI 与发布

`.github/workflows/build.yml` 在每次构建中并行产生：

- `server_jar`：`target/server.jar`
- `server_linux_x64`：`build/native/server`，Linux x86_64

GitHub Actions 会把 build artifact 自动封装成 ZIP 下载，因此 `server_jar`
下载后是包含 `server.jar` 的压缩包，`server_linux_x64` 下载后是包含 `server`
的压缩包。Release 流程会下载并解压这些 artifact，再发布原始文件。

原生任务还会执行编译警告检查、嵌入资源检查和伪终端启动冒烟测试。

`.github/workflows/release.yml` 接收一次成功构建的 run ID 和版本号，计算两个
文件的 SHA-256 后发布为：

```text
server_jar__<sha256>.jar
server_linux_x64__<sha256>
SHA256SUMS
```
