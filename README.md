# jar-console-bridge

`jar-console-bridge` 是一个轻量 Java jar。它用于只有 `java -jar server.jar` 启动能力的游戏面板，
把面板的 stdin、stdout、stderr 连接到交互式 shell，并在后台运行 `server__autoLaunch.sh`。

项目优先使用 Bash。如果 Bash 不存在，会自动回退到 `sh`。

## 功能

- 将面板控制台连接到交互式 Bash 或 sh
- 在后台运行 `server__autoLaunch.sh`
- 使用 `server__path.sh` 统一配置交互式 shell 和后台脚本的 PATH
- 自动从 jar 所在目录定位或释放启动脚本
- 构建产物固定为 `target/server.jar`

## 编译

Windows：

```powershell
.\mvnw.cmd clean package
```

Linux 或 macOS：

```bash
./mvnw clean package
```

编译结果：

```text
target/server.jar
```
