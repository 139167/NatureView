# NatureCore Architecture

NatureView 的首要边界是静态世界定义与动态自然状态分离。

```text
WorldDefinition
  地形、河流、生物群系、铁路等静态规则结果
          |
          v
NatureSimulation
  时间、天气、风、光照、水文与事件
          |
          v
Unified Snapshot
          |
     +----+----+
     |         |
 Renderer    Audio
```

## Invariants

1. 静态世界由 seed 唯一确定。
2. 动态自然使用 20 Hz 固定 tick；渲染帧率不改变模拟结果。
3. 渲染器、音频和相机只读取同一模拟状态。
4. 视觉粒子与声音不能创建独立随机风场。
5. 核心不依赖图形 API、窗口系统或音频设备。
6. 软件预览只读取核心快照，不向核心写回渲染状态。

## M0 Scope

M0 只证明世界生成、环境规则、相机路径与确定性。

明确不包含：

- PBR、GI、阴影或植被渲染；
- 音频解码、空间化或混音；
- DX12、Win32 或壁纸宿主；
- 铁路、列车、天气事件编辑和美术资源管线。

## Next Seam

DX12 宿主通过只读快照接入核心。若渲染需要高频风场，GPU 表现层可从快照频率和相位重建细节，但低频风矢量必须来自 `EnvironmentState`。
