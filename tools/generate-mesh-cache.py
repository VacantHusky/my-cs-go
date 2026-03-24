#!/usr/bin/env python3

from __future__ import annotations

import json
import math
import os
import struct
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


ROOT_DIR = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT_DIR / "assets"
SOURCE_ROOT = ASSET_ROOT / "source"
CACHE_ROOT = ASSET_ROOT / "generated" / "mesh_cache"
MESH_MAGIC = 0x314D4353
MESH_VERSION = 2


def sanitize_path_key(value: str) -> str:
    out = []
    for character in value:
        if character.isalnum() or character in "._-":
            out.append(character)
        else:
            out.append("_")
    return "".join(out)


def cache_path_for(source_path: Path) -> Path:
    try:
        relative = source_path.relative_to(ASSET_ROOT)
    except ValueError:
        relative = source_path.name
    return CACHE_ROOT / f"{sanitize_path_key(relative.as_posix())}.meshbin"


def normalize(vector: Sequence[float]) -> Tuple[float, float, float]:
    length = math.sqrt(sum(component * component for component in vector))
    if length <= 1e-6:
        return (0.0, 1.0, 0.0)
    return tuple(component / length for component in vector)  # type: ignore[return-value]


def subtract(a: Sequence[float], b: Sequence[float]) -> Tuple[float, float, float]:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a: Sequence[float], b: Sequence[float]) -> Tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def path_color(source_path: Path, material_name: str = "") -> Tuple[float, float, float]:
    key = f"{source_path.as_posix()} {material_name}".lower()
    if "barrel" in key:
        return (0.62, 0.38, 0.30)
    if "crate" in key or "wood" in key:
        return (0.66, 0.50, 0.32)
    if "knife" in key or "bayonet" in key:
        return (0.74, 0.76, 0.80)
    if "flash" in key:
        return (0.92, 0.92, 0.86)
    if "smoke" in key:
        return (0.60, 0.64, 0.68)
    if "grenade" in key or "frag" in key:
        return (0.34, 0.44, 0.30)
    if "sniper" in key:
        return (0.26, 0.28, 0.30)
    if "rifle" in key or "shotgun" in key or "submachine" in key or "gun" in key:
        return (0.32, 0.34, 0.36)
    return (0.78, 0.80, 0.84)


def write_mesh_cache(source_path: Path, vertices: List[Tuple[float, ...]]) -> None:
    if not vertices:
        return

    xs = [vertex[0] for vertex in vertices]
    ys = [vertex[1] for vertex in vertices]
    zs = [vertex[2] for vertex in vertices]
    center = ((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5, (min(zs) + max(zs)) * 0.5)
    radius = max(
        math.sqrt((vertex[0] - center[0]) ** 2 + (vertex[1] - center[1]) ** 2 + (vertex[2] - center[2]) ** 2)
        for vertex in vertices
    )
    out_path = cache_path_for(source_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with out_path.open("wb") as stream:
        stream.write(struct.pack("<III", MESH_MAGIC, MESH_VERSION, len(vertices)))
        stream.write(struct.pack("<ffff", center[0], center[1], center[2], radius))
        for vertex in vertices:
            stream.write(struct.pack("<fffffffffff", *vertex))


def parse_obj_vertex_index(token: str, vertex_count: int) -> int:
    index_token = token.split("/")[0]
    if not index_token:
        return -1
    raw = int(index_token)
    if raw > 0:
        return raw - 1
    if raw < 0:
        return vertex_count + raw
    return -1


def load_obj_material_colors(path: Path) -> Dict[str, Tuple[float, float, float]]:
    colors: Dict[str, Tuple[float, float, float]] = {}
    if not path.exists():
        return colors

    current_material = ""
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("newmtl "):
            current_material = line[7:].strip()
        elif current_material and line.startswith("Kd "):
            parts = line[3:].split()
            if len(parts) >= 3:
                colors[current_material] = (float(parts[0]), float(parts[1]), float(parts[2]))
    return colors


def build_obj_mesh(source_path: Path) -> List[Tuple[float, ...]]:
    positions: List[Tuple[float, float, float]] = []
    normals: List[Tuple[float, float, float]] = []
    texcoords: List[Tuple[float, float]] = []
    vertices: List[Tuple[float, ...]] = []
    material_colors: Dict[str, Tuple[float, float, float]] = {}
    current_color = path_color(source_path)

    for line in source_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("mtllib "):
            material_colors = load_obj_material_colors(source_path.parent / line[7:].strip())
        elif line.startswith("usemtl "):
            current_color = material_colors.get(line[7:].strip(), path_color(source_path, line[7:].strip()))
        elif line.startswith("v "):
            _, x, y, z = line.split(maxsplit=3)
            positions.append((float(x), float(y), float(z)))
        elif line.startswith("vn "):
            _, x, y, z = line.split(maxsplit=3)
            normals.append(normalize((float(x), float(y), float(z))))
        elif line.startswith("vt "):
            parts = line.split()
            if len(parts) >= 3:
                texcoords.append((float(parts[1]), 1.0 - float(parts[2])))
        elif line.startswith("f "):
            tokens = line[2:].split()
            if len(tokens) < 3:
                continue
            parsed = []
            for token in tokens:
                fields = token.split("/")
                position_index = parse_obj_vertex_index(token, len(positions))
                texcoord_index = -1
                normal_index = -1
                if len(fields) >= 2 and fields[1]:
                    texcoord_index = int(fields[1]) - 1 if int(fields[1]) > 0 else len(texcoords) + int(fields[1])
                if len(fields) >= 3 and fields[2]:
                    normal_index = int(fields[2]) - 1 if int(fields[2]) > 0 else len(normals) + int(fields[2])
                parsed.append((position_index, texcoord_index, normal_index))
            for index in range(1, len(parsed) - 1):
                triangle = [parsed[0], parsed[index], parsed[index + 1]]
                tri_positions = [positions[item[0]] for item in triangle if 0 <= item[0] < len(positions)]
                if len(tri_positions) != 3:
                    continue
                face_normal = normalize(cross(subtract(tri_positions[1], tri_positions[0]), subtract(tri_positions[2], tri_positions[0])))
                for (position_index, texcoord_index, normal_index), position in zip(triangle, tri_positions):
                    normal = face_normal
                    if 0 <= normal_index < len(normals):
                        normal = normals[normal_index]
                    texcoord = (0.0, 0.0)
                    if 0 <= texcoord_index < len(texcoords):
                        texcoord = texcoords[texcoord_index]
                    vertices.append((*position, *normal, *texcoord, *current_color))
    return vertices


def accessor_components(accessor_type: str) -> int:
    return {
        "SCALAR": 1,
        "VEC2": 2,
        "VEC3": 3,
        "VEC4": 4,
    }.get(accessor_type, 1)


def component_struct(component_type: int) -> Tuple[str, int]:
    mapping = {
        5123: ("H", 2),
        5125: ("I", 4),
        5126: ("f", 4),
    }
    return mapping[component_type]


def read_accessor(gltf: dict, binary: bytes, accessor_index: int) -> List[Tuple[float, ...]]:
    accessor = gltf["accessors"][accessor_index]
    buffer_view = gltf["bufferViews"][accessor["bufferView"]]
    component_format, component_size = component_struct(accessor["componentType"])
    component_count = accessor_components(accessor["type"])
    count = accessor["count"]
    stride = buffer_view.get("byteStride", component_size * component_count)
    offset = buffer_view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    values = []
    for index in range(count):
        start = offset + index * stride
        end = start + component_size * component_count
        raw = binary[start:end]
        values.append(struct.unpack("<" + component_format * component_count, raw))
    return values


def node_transform(node: dict) -> Tuple[Tuple[float, float, float], Tuple[float, float, float]]:
    translation = tuple(node.get("translation", [0.0, 0.0, 0.0]))
    scale = tuple(node.get("scale", [1.0, 1.0, 1.0]))
    return translation, scale


def apply_node_transform(position: Sequence[float], translation: Sequence[float], scale: Sequence[float]) -> Tuple[float, float, float]:
    return (
        position[0] * scale[0] + translation[0],
        position[1] * scale[1] + translation[1],
        position[2] * scale[2] + translation[2],
    )


def build_gltf_mesh(source_path: Path) -> List[Tuple[float, ...]]:
    gltf = json.loads(source_path.read_text(encoding="utf-8"))
    buffer_uri = gltf["buffers"][0]["uri"]
    binary = (source_path.parent / buffer_uri).read_bytes()
    scene = gltf["scenes"][gltf.get("scene", 0)]
    nodes = gltf["nodes"]
    meshes = gltf["meshes"]
    vertices: List[Tuple[float, ...]] = []

    for node_index in scene.get("nodes", []):
        node = nodes[node_index]
        mesh_index = node.get("mesh")
        if mesh_index is None:
            continue
        translation, scale = node_transform(node)
        for primitive in meshes[mesh_index].get("primitives", []):
            attributes = primitive["attributes"]
            positions = read_accessor(gltf, binary, attributes["POSITION"])
            normals = read_accessor(gltf, binary, attributes.get("NORMAL", attributes["POSITION"]))
            texcoords = read_accessor(gltf, binary, attributes["TEXCOORD_0"]) if "TEXCOORD_0" in attributes else []
            indices = read_accessor(gltf, binary, primitive["indices"])
            material_name = ""
            material_index = primitive.get("material")
            if material_index is not None and material_index < len(gltf.get("materials", [])):
                material_name = gltf["materials"][material_index].get("name", "")
            color = path_color(source_path, material_name)
            flat_indices = [int(index[0]) for index in indices]
            for start in range(0, len(flat_indices), 3):
                tri_indices = flat_indices[start:start + 3]
                if len(tri_indices) < 3:
                    continue
                tri_positions = [apply_node_transform(positions[idx], translation, scale) for idx in tri_indices]
                tri_normals = [normalize(normals[idx]) for idx in tri_indices]
                if len(tri_positions) != 3:
                    continue
                fallback_normal = normalize(cross(subtract(tri_positions[1], tri_positions[0]), subtract(tri_positions[2], tri_positions[0])))
                for local_index, (position, normal) in enumerate(zip(tri_positions, tri_normals)):
                    final_normal = normal if any(abs(component) > 1e-4 for component in normal) else fallback_normal
                    texcoord = (0.0, 0.0)
                    accessor_index = tri_indices[local_index]
                    if accessor_index < len(texcoords):
                        texcoord = (float(texcoords[accessor_index][0]), 1.0 - float(texcoords[accessor_index][1]))
                    vertices.append((*position, *final_normal, *texcoord, *color))
    return vertices


def collect_sources() -> Iterable[Path]:
    for extension in ("*.obj", "*.gltf"):
        yield from SOURCE_ROOT.rglob(extension)


def main() -> int:
    generated = 0
    for source_path in sorted(collect_sources()):
        if source_path.name.endswith(":Zone.Identifier"):
            continue
        if source_path.suffix.lower() == ".obj":
            vertices = build_obj_mesh(source_path)
        else:
            vertices = build_gltf_mesh(source_path)
        write_mesh_cache(source_path, vertices)
        generated += 1

    print(f"Generated {generated} mesh cache files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
