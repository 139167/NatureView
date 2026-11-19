# NatureView

真实优先的实时自然景观运行时。当前阶段是 Option C：先构建不依赖 UE 或 Unity 的确定性自然核心，再接入自写图形宿主。

## 当前进度

首个 M0 切片已包含：

- 确定性山地河谷与河流几何生成；
- 统一天气、风场、光照、河流流量与太阳状态；
- 沿河谷岸边的慢速相机路径；
- 20 Hz 固定步长自然模拟；
- 渲染、音频和相机共同读取的状态快照；
- 60 秒 headless 模拟工具；
- 从统一快照生成地形、河面与天空的软件预览；
- 生成分块地形与连续河面网格，并筛选相机可见地形块；
- macOS AppKit 窗口、Metal 网格上传、基础光照与实时相机；
- HDR 离屏渲染、解析天空、太阳光晕、曝光与 ACES tone mapping；
- 2048² 方向光深度阴影图与 3×3 PCF；
- 水面双尺度流动法线、天空反射、Fresnel、太阳闪烁与岸边泡沫；
- 纬度、夜间、云量与天气驱动的极光活动状态和 HDR 帘幕；
- 世界与模拟确定性自动测试。

当前不包含植被几何、场景级水面反射与折射、音频播放、极光体积分层、资源管线与桌面壁纸宿主。这些属于后续里程碑。

## 构建

需要 CMake 3.24 以上和支持 C++20 的编译器：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

运行 60 秒模拟：

```sh
./build/naturecore_sim --seed 139167 --seconds 60 --fps 60
```

输出为 JSON。同一构建中的相同 seed、时长和固定 tick 应产生相同世界指纹与最终状态指纹。

生成 60 秒位置的软件预览：

```sh
./build/nature_preview_cli --seed 139167 --seconds 60 \
  --width 960 --height 540 --output build/nature-preview.ppm
```

软件预览只用于验证世界与相机数据。它不追求电影级画质，也不是最终渲染器。

预览 CLI 同时输出渲染场景统计，包括地形块、可见块、顶点、三角形和场景指纹。

### Windows D3D12

Windows 构建会额外生成 `natureview_dx12_probe`。该探针只验证 DXGI 与 D3D12 设备初始化，优先选择硬件适配器，失败时回退 WARP。

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\natureview_dx12_probe.exe
```

`.github/workflows/windows.yml` 会在 GitHub Windows runner 上编译并执行全部测试。

### macOS Metal

macOS 构建会生成 `natureview_metal`。当前版本显示带程序化地表细节的基础地形与河面：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/natureview_metal --smoke --frames 3 --width 960 --height 540
```

不带 `--frames` 时窗口持续运行，关闭窗口退出。`.github/workflows/macos.yml` 会执行 Metal smoke 测试。当前 shader 在运行时编译；正式管线后续改用预编译 metallib。

捕获视频渲染器的实际像素：

```sh
./build/natureview_metal --frames 1 --seconds 60 \
  --width 960 --height 540 --capture build/metal-frame.ppm
```

捕获文件会由结构化 PPM 测试检查尺寸、颜色变化、植被和天空/水面区域。
捕获尺寸使用物理像素；Retina 屏幕会按 backing scale 放大。

夜间极光验证：

```sh
./build/natureview_metal --seed 22 --frames 1 --seconds 60 \
  --width 960 --height 540 --capture build/aurora-v1.ppm
```

## 结构

```text
include/nature/core/   公共核心接口
src/                   世界生成、环境模型、相机与模拟
tools/                 headless 验证工具
tests/                 确定性与连续性测试
```

## 下一里程碑

当前优先完成 macOS 光影与环境系统：

1. 水面场景反射、水深与折射修正；
2. 极光体积分层、时间演变与更可信的帘幕边缘；
3. 植被撒布、实例化、阴影与统一风场；
4. 音频总线、风/水/雨环境床与空间声源；
5. 长期运行与性能遥测；
6. 完成 Mac 后再迁移 Windows DX12 可绘制宿主。
