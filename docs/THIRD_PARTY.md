## Third-Party Upstreams

This project references CGMiner codebases for historical context and potential adapter/protocol insights. If code is copied or adapted, GPL obligations apply (CGMiner is GPL-licensed).

### Upstreams cloned into `external/`
- `cgminer-kanoi`: https://github.com/kanoi/cgminer — actively maintained ASIC/FPGA focus; GPL. Useful for modern Stratum handling and structure.
- `cgminer-luke`: https://github.com/luke-jr/cgminer — older GPU-era snapshot with OpenCL; GPL. Useful for GPU-related legacy paths.

### Notes
- We aim for a clean, SOLID architecture and CUDA kernels. CGMiner’s OpenCL paths are reference only.
- Direct reuse of CGMiner source files requires this project to comply with GPL. Track any borrowed files/functions explicitly in a table below.

### Borrowed Code Registry (fill as needed)
| Source file/function | Upstream (URL + commit) | Local file | License header retained | Modifications |
|---|---|---|---|---|
|  |  |  |  |  |


