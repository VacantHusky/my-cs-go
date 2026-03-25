这里放可编辑地图源文件。

当前示例地图会在程序启动时自动生成：

- `depot_lab.arena`

地图文件扩展名仍然使用 `.arena`，但内容现在是带版本号的 JSON：

```json
{
  "format": "mycsg.map",
  "version": 3,
  "name": "Depot Lab",
  "size": { "width": 24, "height": 8, "depth": 24 },
  "blocks": [],
  "spawns": [],
  "props": [
    {
      "id": "crate",
      "transform": {
        "position": [7.5, 0.0, 7.5],
        "rotation": [0.0, 45.0, 0.0],
        "scale": [1.0, 1.0, 1.0]
      },
      "render": {
        "model": "assets/source/props/crate.glb",
        "material": "assets/generated/materials/default.mat"
      }
    }
  ],
  "lights": []
}
```

读取端兼容旧版逐行文本 `.arena`，保存时统一写出新版 JSON。
