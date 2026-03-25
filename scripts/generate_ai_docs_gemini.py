import os
from pathlib import Path
import google.generativeai as genai

# Setup
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

model = genai.GenerativeModel("gemini-1.5-flash")

xml_dir = Path("docs/xml")
out_dir = Path("docs/ai")
out_dir.mkdir(parents=True, exist_ok=True)

# Collect XML context (limit size to avoid overload)
chunks = []
for path in list(xml_dir.glob("*.xml"))[:25]:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
        chunks.append(f"\n--- {path.name} ---\n{text[:12000]}")
    except:
        pass

context = "\n".join(chunks)

prompt = f"""
You are generating developer documentation for a C++ game project.

Using the Doxygen XML below, create:

1. architecture.html
2. contributor-guide.html
3. index.html

RULES:
- DO NOT hallucinate missing systems
- Only describe what exists
- Keep explanations practical and dev-focused
- Output EXACTLY in this format:

===FILE: architecture.html===
<html>...</html>
===FILE: contributor-guide.html===
<html>...</html>
===FILE: index.html===
<html>...</html>

Context:
{context}
"""

response = model.generate_content(prompt)

text = response.text

# Parse output
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

# Save files
for name in ["architecture.html", "contributor-guide.html", "index.html"]:
    content = files.get(name, f"<html><body><h1>{name}</h1><p>Generation failed.</p></body></html>")
    (out_dir / name).write_text(content, encoding="utf-8")

print("✅ AI docs generated")
