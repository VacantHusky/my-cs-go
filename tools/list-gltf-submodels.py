#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT_DIR / "assets"
GLB_MAGIC = 0x46546C67
GLB_JSON_CHUNK = 0x4E4F534A


def load_glb_json(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 20:
        raise ValueError(f"GLB file is too small: {path}")

    magic, _version, length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        raise ValueError(f"Invalid GLB magic: {path}")

    offset = 12
    while offset + 8 <= length:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset:offset + chunk_length]
        offset += chunk_length
        if chunk_type == GLB_JSON_CHUNK:
            return json.loads(chunk.decode("utf-8"))

    raise ValueError(f"JSON chunk not found: {path}")


def strip_blender_suffix(name: str) -> str:
    return re.sub(r"\.\d+$", "", name)


def slugify(name: str) -> str:
    slug = []
    for character in name:
        if character.isalnum():
            slug.append(character.lower())
        elif character in {"_", "-"}:
            slug.append(character)
        else:
            slug.append("_")
    return "".join(slug).strip("_")


def category_for(name: str) -> str:
    lowered = name.lower()
    if lowered.startswith("poster_"):
        return "posters"
    if lowered in {"chair", "light", "metro_map", "ticket_machine", "trash_can", "vending_machine"}:
        return "props"
    if lowered.startswith("subway_car"):
        return "train"
    if lowered.startswith("door") or lowered.startswith("glass"):
        return "train_parts"
    if lowered.startswith("seats") or lowered.startswith("railing_v"):
        return "train_interior"
    if lowered.startswith("automatic_ticket_gate"):
        return "station_interactives"
    return "station"


def recommended_output_path(family_name: str) -> str:
    category = category_for(family_name)
    file_name = f"{slugify(family_name)}.gltf"
    return f"assets/source/itchio/metro_psx/split/{category}/{file_name}"


def material_names_for_mesh(mesh: dict, materials: list[dict]) -> list[str]:
    names: list[str] = []
    for primitive in mesh.get("primitives", []):
        material_index = primitive.get("material")
        if material_index is None or material_index < 0 or material_index >= len(materials):
            continue
        material_name = materials[material_index].get("name", f"material_{material_index}")
        if material_name not in names:
            names.append(material_name)
    return names


def build_report(source_path: Path) -> dict:
    gltf = load_glb_json(source_path)
    meshes = gltf.get("meshes", [])
    nodes = gltf.get("nodes", [])
    materials = gltf.get("materials", [])

    node_entries: list[dict] = []
    family_entries: dict[str, dict] = {}
    for node_index, node in enumerate(nodes):
        mesh_index = node.get("mesh")
        if mesh_index is None or mesh_index < 0 or mesh_index >= len(meshes):
            continue

        mesh = meshes[mesh_index]
        mesh_name = mesh.get("name", f"mesh_{mesh_index}")
        node_name = node.get("name", f"node_{node_index}")
        family_name = strip_blender_suffix(mesh_name)
        material_names = material_names_for_mesh(mesh, materials)

        node_entry = {
            "nodeIndex": node_index,
            "nodeName": node_name,
            "meshIndex": mesh_index,
            "meshName": mesh_name,
            "familyName": family_name,
            "category": category_for(family_name),
            "primitiveCount": len(mesh.get("primitives", [])),
            "materials": material_names,
            "recommendedOutput": recommended_output_path(family_name),
        }
        node_entries.append(node_entry)

        family_entry = family_entries.setdefault(family_name, {
            "familyName": family_name,
            "category": category_for(family_name),
            "recommendedOutput": recommended_output_path(family_name),
            "instanceCount": 0,
            "meshNames": [],
            "nodeNames": [],
            "materials": [],
            "primitiveCount": len(mesh.get("primitives", [])),
        })
        family_entry["instanceCount"] += 1
        if mesh_name not in family_entry["meshNames"]:
            family_entry["meshNames"].append(mesh_name)
        if node_name not in family_entry["nodeNames"]:
            family_entry["nodeNames"].append(node_name)
        for material_name in material_names:
            if material_name not in family_entry["materials"]:
                family_entry["materials"].append(material_name)

    family_list = sorted(family_entries.values(), key=lambda item: (item["category"], item["familyName"].lower()))
    node_entries.sort(key=lambda item: (item["category"], item["familyName"].lower(), item["nodeName"].lower()))

    try:
        relative_source = source_path.relative_to(ROOT_DIR)
    except ValueError:
        relative_source = source_path

    return {
        "source": relative_source.as_posix(),
        "totalMeshNodes": len(node_entries),
        "uniqueFamilies": len(family_list),
        "families": family_list,
        "nodes": node_entries,
    }


def write_json_report(path: Path, report: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def write_markdown_report(path: Path, report: dict) -> None:
    lines: list[str] = []
    lines.append("# metro_psx 独立 glTF 子模型列表")
    lines.append("")
    lines.append(f"- 源文件: `{report['source']}`")
    lines.append(f"- 节点级子模型数: `{report['totalMeshNodes']}`")
    lines.append(f"- 去重后的模型族数: `{report['uniqueFamilies']}`")
    lines.append("")
    lines.append("## 推荐拆分清单")
    lines.append("")
    lines.append("| 类别 | 模型族 | 实例数 | 面片组数 | 材质 | 推荐输出 |")
    lines.append("| --- | --- | ---: | ---: | --- | --- |")
    for family in report["families"]:
        materials = ", ".join(family["materials"]) if family["materials"] else "-"
        lines.append(
            f"| {family['category']} | {family['familyName']} | {family['instanceCount']} | "
            f"{family['primitiveCount']} | {materials} | `{family['recommendedOutput']}` |")

    lines.append("")
    lines.append("## 节点级拆分明细")
    lines.append("")
    for node in report["nodes"]:
        materials = ", ".join(node["materials"]) if node["materials"] else "-"
        lines.append(
            f"- `{node['nodeName']}` -> 族 `{node['familyName']}`"
            f" | mesh=`{node['meshName']}`"
            f" | 类别=`{node['category']}`"
            f" | 材质=`{materials}`"
            f" | 输出=`{node['recommendedOutput']}`")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="List split-ready submodels from a GLB file.")
    parser.add_argument("source", type=Path, help="Source .glb file")
    parser.add_argument("--json-out", type=Path, required=True, help="Output JSON report path")
    parser.add_argument("--md-out", type=Path, required=True, help="Output Markdown report path")
    args = parser.parse_args()

    source_path = args.source
    if not source_path.is_absolute():
        source_path = ROOT_DIR / source_path

    report = build_report(source_path)
    write_json_report(args.json_out if args.json_out.is_absolute() else ROOT_DIR / args.json_out, report)
    write_markdown_report(args.md_out if args.md_out.is_absolute() else ROOT_DIR / args.md_out, report)
    print(
        f"Wrote submodel reports: families={report['uniqueFamilies']} nodes={report['totalMeshNodes']} "
        f"json={args.json_out} md={args.md_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
