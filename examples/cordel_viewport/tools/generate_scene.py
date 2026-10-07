#!/usr/bin/env python3
"""Generate the CC0 CORDEL fixture, without external assets or dependencies."""

import argparse
import base64
import json
from pathlib import Path
import struct

SCENE = (
    ("ground", (0, -.15, -4), (18, .3, 22), (.22, .27, .3, 1)),
    ("near_red", (-1.8, 1, -2), (2, 2, 2), (.9, .12, .08, 1)),
    ("far_blue", (-1.8, 1.5, -6), (3, 3, 2), (.08, .3, .95, 1)),
    ("tall_gold", (3, 2.5, -5), (1.2, 5, 1.2), (.95, .64, .08, 1)),
    ("green", (-4, .65, -1), (1.3, 1.3, 1.3), (.12, .75, .3, 1)),
    ("small_cream", (1.2, .5, -1), (1, 1, 1), (.9, .86, .6, 1)),
    ("distant_purple", (1.2, 1, -10), (2, 2, 2), (.62, .15, .7, 1)),
)


def build():
    blob = bytearray()
    views, accessors, meshes, nodes, materials = [], [], [], [], []

    def accessor(values, width, component, target, bounds=False):
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        flat = [v for row in values for v in row]
        blob.extend(struct.pack("<" + ("f" if component == 5126 else "H") * len(flat), *flat))
        views.append(dict(buffer=0, byteOffset=offset, byteLength=len(blob) - offset, target=target))
        result = dict(bufferView=len(views)-1, componentType=component, count=len(values),
                      type={1: "SCALAR", 3: "VEC3"}[width])
        if bounds:
            result.update(min=[min(row[i] for row in values) for i in range(width)],
                          max=[max(row[i] for row in values) for i in range(width)])
        accessors.append(result)
        return len(accessors) - 1

    faces = (
        ((0, 0, 1), ((-1,-1,1), (1,-1,1), (1,1,1), (-1,1,1))),
        ((0, 0,-1), ((1,-1,-1), (-1,-1,-1), (-1,1,-1), (1,1,-1))),
        ((1, 0, 0), ((1,-1,1), (1,-1,-1), (1,1,-1), (1,1,1))),
        ((-1,0, 0), ((-1,-1,-1), (-1,-1,1), (-1,1,1), (-1,1,-1))),
        ((0, 1, 0), ((-1,1,1), (1,1,1), (1,1,-1), (-1,1,-1))),
        ((0,-1, 0), ((-1,-1,-1), (1,-1,-1), (1,-1,1), (-1,-1,1))),
    )
    for index, (name, centre, size, colour) in enumerate(SCENE):
        points, normals, triangles = [], [], []
        for normal, corners in faces:
            start = len(points)
            points.extend(tuple(v[i] * size[i] / 2 for i in range(3)) for v in corners)
            normals.extend([normal] * 4)
            triangles.extend([(start+i,) for i in (0, 1, 2, 0, 2, 3)])
        pos = accessor(points, 3, 5126, 34962, True)
        norm = accessor(normals, 3, 5126, 34962)
        indices = accessor(triangles, 1, 5123, 34963)
        meshes.append(dict(name=name, primitives=[dict(attributes=dict(POSITION=pos, NORMAL=norm),
                                                       indices=indices, material=index)]))
        nodes.append(dict(name=name, mesh=index, translation=centre))
        materials.append(dict(name=name, doubleSided=True, pbrMetallicRoughness=dict(
            baseColorFactor=colour, metallicFactor=0, roughnessFactor=1)))
    return dict(asset=dict(version="2.0", generator="CORDEL deterministic Phase 1.1 fixture",
                           copyright="CC0-1.0; procedurally authored for CORDEL ENGINE"),
                scene=0, scenes=[dict(nodes=list(range(len(nodes))))], nodes=nodes, meshes=meshes,
                materials=materials, accessors=accessors, bufferViews=views,
                buffers=[dict(byteLength=len(blob), uri="data:application/octet-stream;base64," +
                              base64.b64encode(blob).decode("ascii"))])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if the checked-in fixture differs")
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1] / "game/assets/cordel_scene.gltf"
    content = json.dumps(build(), indent=2) + "\n"
    if args.check:
        if path.read_text() != content:
            raise SystemExit("Fixture differs: run tools/generate_scene.py")
        print("Deterministic fixture matches")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        print(path)
