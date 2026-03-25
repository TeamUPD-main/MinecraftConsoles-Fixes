import os
from pathlib import Path
from google import genai

api_key = os.environ.get("GOOGLE_API_KEY")
if not api_key:
    raise RuntimeError("GOOGLE_API_KEY is not set")

client = genai.Client(api_key=api_key)

xml_dir = Path("docs/xml")
out_dir = Path("docs/ai")
out_dir.mkdir(parents=True, exist_ok=True)

chunks = []
for path in sorted(xml_dir.glob("*.xml"))[:25]:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
        chunks.append(f"\n--- {path.name} ---\n{text[:12000]}")
    except Exception as e:
        print(f"Skipping {path}: {e}")

context = "\n".join(chunks)

prompt = f"""
You are generating developer documentation for a C++ game project.

Using the Doxygen XML below, create:

1. architecture.html
2. contributor-guide.html
3. index.html

Rules:
- Do not invent systems or behavior not present in the input
- Keep explanations practical and contributor-focused
- Return EXACTLY in this format:

===FILE: architecture.html===
<html>...</html>
===FILE: contributor-guide.html===
<html>...</html>
===FILE: index.html===
<html>...</html>

Context:
{context}
"""

response = client.models.generate_content(
    model="gemini-2.5-flash",
    contents=prompt,
)

text = response.text or ""

current = None
buffer = []
files = {}

for line in text.splitlines():
    if line.startswith("===FILE: ") and line.endswith("==="):
        if current:
            files[current] = "\n".join(buffer).strip()
        current = line[len("===FILE: "):-3]
        buffer = []
    else:
        buffer.append(line)

if current:
    files[current] = "\n".join(buffer).strip()

fallbacks = {
    "architecture.html": """<html><body><h1>Architecture Overview</h1><p>Generation failed.</p></body></html>""",
    "contributor-guide.html": """<html><body><h1>Contributor Guide</h1><p>Generation failed.</p></body></html>""",
    "index.html": """<html><body><h1>AI Documentation</h1><ul><li><a href="architecture.html">Architecture Overview</a></li><li><a href="contributor-guide.html">Contributor Guide</a></li></ul></body></html>""",
}

for name, fallback in fallbacks.items():
    content = files.get(name, fallback)
    (out_dir / name).write_text(content, encoding="utf-8")

print("AI docs generated successfully.")
