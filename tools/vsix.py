#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Packs editors/vscode as a .vsix, the file VS Code's "Install from VSIX..."
takes, so an extension can be installed without copying or linking folders
into VS Code's own.

A .vsix is a zip (the Open Packaging Conventions): [Content_Types].xml,
extension.vsixmanifest, and the extension's files under extension/. This
writes those directly rather than through Microsoft's vsce, which needs
Node; the manifest carries the fields vsce writes for a declarative
extension. Only files git tracks go in.

    vsix.py --out dist        writes dist/termforms-basic-<version>.vsix
"""

import argparse
import json
import subprocess
import sys
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape, quoteattr

ROOT = Path(__file__).resolve().parents[1]
EXTENSION = "editors/vscode"

# Each file kind in the package needs its media type here.
CONTENT_TYPES = {
    ".json": "application/json",
    ".md": "text/markdown",
    ".vsixmanifest": "text/xml",
}


def files() -> list:
    out = subprocess.run(["git", "ls-files", "-z", EXTENSION], cwd=ROOT, check=True,
                         capture_output=True).stdout.decode("utf-8")
    return [name for name in out.split("\0") if name]


def manifest(package: dict) -> str:
    identity = (f'<Identity Language="en-US" Id={quoteattr(package["name"])} '
                f'Version={quoteattr(package["version"])} '
                f'Publisher={quoteattr(package["publisher"])} />')
    engine = quoteattr(package["engines"]["vscode"])
    return f"""<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    {identity}
    <DisplayName>{escape(package["displayName"])}</DisplayName>
    <Description xml:space="preserve">{escape(package["description"])}</Description>
    <Tags>termforms,basic</Tags>
    <Categories>{escape(",".join(package["categories"]))}</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value={engine} />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
      <Property Id="Microsoft.VisualStudio.Code.LocalizedLanguages" Value="" />
    </Properties>
    <License>extension/LICENSE.txt</License>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.License" Path="extension/LICENSE.txt" Addressable="true" />
  </Assets>
</PackageManifest>
"""


def content_types() -> str:
    kinds = dict(CONTENT_TYPES, **{".txt": "text/plain"})
    lines = "".join(f'<Default Extension="{ext}" ContentType="{kind}" />'
                    for ext, kind in sorted(kinds.items()))
    return ('<?xml version="1.0" encoding="utf-8"?>\n'
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            f"{lines}</Types>\n")


def make(out: Path) -> Path:
    package = json.loads((ROOT / EXTENSION / "package.json").read_text(encoding="utf-8"))
    out.mkdir(parents=True, exist_ok=True)
    target = out / f'{package["name"]}-{package["version"]}.vsix'
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as vsix:
        vsix.writestr("[Content_Types].xml", content_types())
        vsix.writestr("extension.vsixmanifest", manifest(package))
        for name in files():
            suffix = Path(name).suffix
            if suffix not in CONTENT_TYPES:
                raise SystemExit(f"{name}: add a content type for {suffix} to tools/vsix.py")
            vsix.write(ROOT / name, "extension/" + name[len(EXTENSION) + 1:])
        vsix.write(ROOT / "LICENSE", "extension/LICENSE.txt")
    return target


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, type=Path)
    print(make(parser.parse_args().out.resolve()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
