#!/usr/bin/env python3
"""Upload markdown content to Feishu docx document."""

import json
import re
import subprocess
import sys

DOCUMENT_ID = "I21BdgmZVo9jvaxxaYycrxUjn4g"
ROOT_BLOCK_ID = DOCUMENT_ID


def run_lark_api(method, path, data=None):
    cmd = ["lark-cli", "api", method, path]
    if data:
        cmd.extend(["--data", json.dumps(data, ensure_ascii=False)])
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"API Error: {result.stderr}", file=sys.stderr)
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        print(f"Parse Error: {result.stdout}", file=sys.stderr)
        return None


def parse_markdown(content):
    lines = content.split('\n')
    blocks = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        if stripped == '---' or (set(stripped) == {'-'} and len(stripped) >= 3):
            blocks.append({"type": "divider"})
            i += 1
            continue

        if stripped.startswith('```'):
            lang = stripped[3:].strip()
            code_lines = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code_lines.append(lines[i])
                i += 1
            i += 1
            blocks.append({"type": "code", "language": lang, "content": '\n'.join(code_lines)})
            continue

        heading_match = re.match(r'^(#{1,6})\s+(.+)$', stripped)
        if heading_match:
            level = len(heading_match.group(1))
            text = heading_match.group(2)
            blocks.append({"type": "heading", "level": level, "content": text})
            i += 1
            continue

        if '|' in stripped:
            table_lines = []
            while i < len(lines) and '|' in lines[i]:
                table_lines.append(lines[i])
                i += 1
            data_lines = [l for l in table_lines if not re.match(r'^\s*\|[\s\-:|]+\|\s*$', l)]
            if data_lines:
                rows = []
                for tl in data_lines:
                    cells = [c.strip() for c in tl.split('|')[1:-1]]
                    rows.append(cells)
                blocks.append({"type": "table", "rows": rows})
            continue

        if stripped.startswith('>'):
            quote_lines = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                quote_lines.append(lines[i].strip()[1:].strip())
                i += 1
            blocks.append({"type": "quote", "content": '\n'.join(quote_lines)})
            continue

        bullet_match = re.match(r'^[\*\-]\s+(.+)$', stripped)
        if bullet_match:
            items = []
            while i < len(lines):
                sl = lines[i].strip()
                if sl.startswith('* ') or sl.startswith('- '):
                    items.append(sl[2:])
                    i += 1
                elif sl and not sl.startswith('#') and not sl.startswith('|') and not sl.startswith('```'):
                    if items:
                        items[-1] += '\n' + sl
                    i += 1
                else:
                    break
            blocks.append({"type": "bullet_list", "items": items})
            continue

        num_match = re.match(r'^(\d+)\.\s+(.+)$', stripped)
        if num_match:
            items = []
            while i < len(lines):
                sl = lines[i].strip()
                nm = re.match(r'^(\d+)\.\s+(.+)$', sl)
                if nm:
                    items.append(nm.group(2))
                    i += 1
                elif sl and not sl.startswith('#') and not sl.startswith('|') and not sl.startswith('```'):
                    if items:
                        items[-1] += '\n' + sl
                    i += 1
                else:
                    break
            blocks.append({"type": "ordered_list", "items": items})
            continue

        para_lines = []
        while i < len(lines):
            sl = lines[i].strip()
            if not sl:
                break
            if sl.startswith('#') or sl.startswith('```') or sl.startswith('---'):
                break
            if sl.startswith('* ') or sl.startswith('- ') or re.match(r'^\d+\.', sl):
                break
            if '|' in sl and sl.startswith('|'):
                break
            para_lines.append(lines[i])
            i += 1

        para_text = '\n'.join(para_lines).strip()
        if para_text:
            blocks.append({"type": "paragraph", "content": para_text})

    return blocks


def inline_elements(text):
    elements = []
    pos = 0
    patterns = [
        (r'`([^`]+)`', 'inline_code'),
        (r'\*\*([^\*]+)\*\*', 'bold'),
        (r'\*([^\*]+)\*', 'italic'),
    ]

    while pos < len(text):
        best_match = None
        best_start = len(text)
        best_type = None

        for pattern, style_name in patterns:
            m = re.search(pattern, text[pos:])
            if m and m.start() + pos < best_start:
                best_start = m.start() + pos
                best_match = m
                best_type = style_name

        if best_match and best_start < len(text):
            if best_start > pos:
                elements.append({"text_run": {"content": text[pos:best_start], "text_element_style": {}}})
            content = best_match.group(1)
            style = {best_type: True}
            elements.append({"text_run": {"content": content, "text_element_style": style}})
            pos = best_start + len(best_match.group(0))
        else:
            if pos < len(text):
                elements.append({"text_run": {"content": text[pos:], "text_element_style": {}}})
            break

    if not elements:
        elements.append({"text_run": {"content": text, "text_element_style": {}}})

    return elements


def feishu_block(md_block):
    btype = md_block["type"]

    if btype == "heading":
        level = md_block["level"]
        block_type = 2 + level
        if block_type > 8:
            block_type = 8
        return {
            "block_type": block_type,
            "heading": {
                "elements": inline_elements(md_block["content"]),
                "style": {}
            }
        }

    elif btype == "paragraph":
        return {
            "block_type": 2,
            "text": {
                "elements": inline_elements(md_block["content"]),
                "style": {}
            }
        }

    elif btype == "code":
        lang_map = {
            "": 0, "plain": 0, "text": 0,
            "cpp": 1, "c++": 1, "c": 2,
            "python": 3, "py": 3,
            "java": 4,
            "js": 5, "javascript": 5,
            "typescript": 6, "ts": 6,
            "go": 7,
            "rust": 8,
            "bash": 9, "shell": 9, "sh": 9,
            "sql": 10,
            "yaml": 11, "yml": 11,
            "xml": 12,
            "json": 13,
            "html": 14,
            "css": 15,
            "markdown": 16, "md": 16,
        }
        lang = md_block.get("language", "").lower()
        lang_code = lang_map.get(lang, 0)
        return {
            "block_type": 17,
            "code": {
                "elements": [{"text_run": {"content": md_block["content"], "text_element_style": {}}}],
                "style": {},
                "language": lang_code
            }
        }

    elif btype == "divider":
        return {"block_type": 18, "divider": {}}

    elif btype == "quote":
        return {
            "block_type": 13,
            "quote": {
                "elements": inline_elements(md_block["content"]),
                "style": {}
            }
        }

    elif btype == "bullet_list":
        content = '\n'.join(f"• {item}" for item in md_block["items"])
        return {
            "block_type": 2,
            "text": {
                "elements": inline_elements(content),
                "style": {}
            }
        }

    elif btype == "ordered_list":
        content = '\n'.join(f"{i+1}. {item}" for i, item in enumerate(md_block["items"]))
        return {
            "block_type": 2,
            "text": {
                "elements": inline_elements(content),
                "style": {}
            }
        }

    elif btype == "table":
        rows = md_block["rows"]
        if not rows:
            return None
        num_cols = max(len(r) for r in rows)
        num_rows = len(rows)

        table_block = {
            "block_type": 20,
            "table": {
                "property": {
                    "row_size": num_rows,
                    "column_size": num_cols,
                    "merge_info": []
                },
                "children": []
            }
        }

        for row_idx, row in enumerate(rows):
            for col_idx, cell_text in enumerate(row):
                cell_block = {
                    "block_type": 21,
                    "table_cell": {
                        "children": [{
                            "block_type": 2,
                            "text": {
                                "elements": inline_elements(cell_text),
                                "style": {}
                            }
                        }]
                    }
                }
                table_block["table"]["children"].append(cell_block)

        return table_block

    return None


def upload_blocks(blocks):
    batch_size = 20
    total = len(blocks)

    for i in range(0, total, batch_size):
        batch = blocks[i:i+batch_size]
        feishu_children = []

        for b in batch:
            fb = feishu_block(b)
            if fb:
                feishu_children.append(fb)

        if not feishu_children:
            continue

        data = {"children": feishu_children}
        result = run_lark_api(
            "POST",
            f"/open-apis/docx/v1/documents/{DOCUMENT_ID}/blocks/{ROOT_BLOCK_ID}/children",
            data
        )

        if result and result.get("code") == 0:
            print(f"Uploaded blocks {i+1}-{min(i+batch_size, total)} / {total}")
        else:
            print(f"Failed at blocks {i+1}-{min(i+batch_size, total)}: {result}", file=sys.stderr)
            return False

    return True


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <markdown_file>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r', encoding='utf-8') as f:
        content = f.read()

    print(f"Parsing markdown: {len(content)} chars, {len(content.split(chr(10)))} lines")
    blocks = parse_markdown(content)
    print(f"Parsed into {len(blocks)} blocks")

    print("Uploading to Feishu...")
    if upload_blocks(blocks):
        print("Upload complete!")
    else:
        print("Upload failed!", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
