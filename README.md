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
- 世界与模拟确定性自动测试。

当前不包含图形、音频播放、资源管线、DX12 与 Windows 壁纸宿主。这些属于下一个里程碑。

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

输出为 JSON。相同 seed、时长和固定 tick 应产生相同世界指纹与最终状态指纹。

## 结构

```text
include/nature/core/   公共核心接口
src/                   世界生成、环境模型、相机与模拟
tools/                 headless 验证工具
tests/                 确定性与连续性测试
```

## 下一里程碑

下一个切片是 Windows DX12 渲染宿主，只读取 `NatureSimulation` 快照：

1. 渲染河网、高度场和基础地形材质；
2. 使用同一太阳与雾状态生成方向光及大气；
3. 显示相机路径和 60 秒运动；
4. 加入帧时间、显存与 p99 性能遥测；
5. 通过截图与长镜头验证时间稳定性。
