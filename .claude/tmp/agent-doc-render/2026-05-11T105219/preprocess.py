#!/usr/bin/env python3
"""把 agent_design.md 中所有 ```mermaid 代码块抽取到单独的 .mmd 文件，
并把原位置替换成 <whiteboard type="blank"></whiteboard> 占位。
输出：processed.md, diagram_1.mmd ~ diagram_N.mmd
"""
import re
from pathlib import Path

src = Path("/mnt/c/Users/admin/ros2/design/layer_01_ai/agent_design.md")
out_dir = Path("/mnt/c/Users/admin/ros2/.claude/tmp/agent-doc-render/2026-05-11T105219")
out_dir.mkdir(parents=True, exist_ok=True)

content = src.read_text(encoding="utf-8")

# 匹配 ```mermaid 至 ``` 的代码块（多行，非贪婪）
pattern = re.compile(r"```mermaid\n(.*?)\n```", re.DOTALL)

mermaids = []
def replace(match):
    body = match.group(1)
    mermaids.append(body)
    return '<whiteboard type="blank"></whiteboard>'

processed = pattern.sub(replace, content)

for i, body in enumerate(mermaids, 1):
    (out_dir / f"diagram_{i}.mmd").write_text(body + "\n", encoding="utf-8")

(out_dir / "processed.md").write_text(processed, encoding="utf-8")

print(f"mermaid count: {len(mermaids)}")
print(f"processed.md length: {len(processed)} chars")
for i in range(len(mermaids)):
    fp = out_dir / f"diagram_{i+1}.mmd"
    print(f"  diagram_{i+1}.mmd  ({fp.stat().st_size} bytes)")
