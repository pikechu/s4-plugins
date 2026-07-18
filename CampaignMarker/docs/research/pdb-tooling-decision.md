# PDB tooling decision

## Inputs

- PDB: `S4_MainR.pdb`, identified by `research/evidence/manifest.sha256`.
- Image: `S4_Main.exe` 2.50.1516.0, identified by the same manifest.
- Required output: symbol kind, rendered name, CodeView section/offset, section
  headers, and type information when present.

## Local audit

No `llvm-pdbutil.exe`, `cvdump.exe`, or `dia2dump.exe` was installed. Three
copies of `msdia140.dll` were found beneath Visual Studio Installer feedback
components (x86, amd64, and arm64), but `Microsoft.DiaSource` was not
registered. Registering that COM server would modify system state, and no DIA
SDK sample executable was present.

Plain `strings` was rejected: it exposes some symbol text but cannot reliably
identify the symbol record kind, section, address, or type relationship.

## Decision

Use a repository-local extraction of Ubuntu's pinned LLVM 14.0.6 packages.
Nothing is installed system-wide; the packages are extracted beneath ignored
`research/vendor/llvm-pdbutil`.

Packages:

- `llvm-14_14.0.6-19build4_amd64.deb`
  - URL: `http://archive.ubuntu.com/ubuntu/pool/universe/l/llvm-toolchain-14/llvm-14_14.0.6-19build4_amd64.deb`
  - SHA-256: `2e34a19fa9b4d6c1fd9d1a06f25421897592c02ebabca1a94e86aa0e535bc187`
- `libllvm14t64_14.0.6-19build4_amd64.deb`
  - URL: `http://archive.ubuntu.com/ubuntu/pool/universe/l/llvm-toolchain-14/libllvm14t64_14.0.6-19build4_amd64.deb`
  - SHA-256: `610cf473a9d76a3cb5fe4d6579ba3de0812690e53d41315aba0c9b1a9a94c782`

Selected executable:

```text
research/vendor/llvm-pdbutil/usr/lib/llvm-14/bin/llvm-pdbutil
```

`llvm-pdbutil dump --publics --globals --modules --section-headers
--section-contribs` successfully exposes public function/code/data records and
their `section:offset` addresses. This PDB has no usable TPI records in the
`--types` output, so type recovery must rely on public symbols, RTTI names,
image analysis, and later runtime validation. An address from this evidence is
valid only for the exact executable/PDB hashes recorded by the collector.
