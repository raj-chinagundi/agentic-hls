# agentic-hls

An agentic pipeline that takes a C++ benchmark, profiles it, and automatically generates HLS-optimised code ready for FPGA synthesis — no manual steps required.

---

## How it works

```
python main.py
└── enter application name (e.g. "gemm")
```

The top function is looked up from `TOP_FUNCTION_MAP` in `main.py` — no manual entry needed.

### Pipeline flow

```
benchmark/fir/fir.cpp
        │
        ▼
1. PROFILING (gprof)
   Compiles and runs the benchmark, generates a gprof performance report.
   Output → profiling_report/fir/
        │
        ▼
2. BOTTLENECK ANALYSIS (LLM)
   LLM reads the source code + gprof report, identifies performance
   bottlenecks and scores each function on execution time, parallelism,
   compute intensity, and data intensity.
   Output → bottleneck_report/fir/
        │
        ▼
3. TASK PIPELINING (LLM)
   LLM restructures the algorithm into HLS dataflow pipeline stages,
   splitting loops and functions so they can run concurrently on FPGA.
   Output → pipeline/gpt-4o/fir/
        │
        ▼
4. HLS OPTIMISATION (LLM)
   LLM selects the most suitable HLS pragmas (PIPELINE, UNROLL,
   ARRAY_PARTITION, DATAFLOW, etc.) and applies them to the staged code.
   Output → stage_opt/gpt-4o/fir/
        │
        ▼
5. C SIMULATION (vitis_hls via TCL)
   The optimised .cpp is compiled and run as a C++ program to verify
   it is functionally correct. Runs inside Vivado 2022.1.
   Output → hls_runs/fir/<timestamp>/csim_retry_0/
        │
        ├── FAILED? ──► back to step 4 (up to 3 retries)
        │               The csim error log is fed back to the LLM
        │               so it can fix the code before retrying.
        │
        ▼ PASSED
6. C SYNTHESIS (vitis_hls via TCL)
   The verified optimised code is synthesised to RTL. Generates timing
   and resource estimates for the target FPGA.
   Output → hls_runs/fir/<timestamp>/csynth/
        │
        ▼
7. RESULTS
   Synthesis report (XML) is parsed and key metrics are saved.
   Output → results/fir/
```

### What gets saved

| Folder | Contents |
|--------|----------|
| `profiling_report/` | gprof text reports |
| `bottleneck_report/` | LLM bottleneck analysis |
| `pipeline/` | LLM-generated dataflow-staged C++ |
| `stage_opt/` | LLM-optimised C++ with HLS pragmas |
| `hls_runs/` | TCL scripts, csim/csynth logs, synthesis project |
| `results/` | Final summary: latency, LUT, FF, BRAM, DSP |

---

## Benchmarks

| Benchmark | Enter at prompt | Top function (from `TOP_FUNCTION_MAP`) |
|---|---|---|
| FIR Filter | `fir` | `fir` |
| Matrix Multiply (GEMM) | `gemm` | `gemm` |
| 2D Stencil | `stencil` | `stencil` |
| FFT (strided radix-2) | `fft` | `fft` |
| Merge Sort | `sort` | `ms_mergesort` |
| Needleman-Wunsch Alignment | `nw` | `needwun` |
| Backpropagation | `backprop` | `backprop` |
| BFS (queue-based) | `bfs` | `bfs` |
| AES-256 ECB | `aes` | `aes256_encrypt_ecb` |

---

## Config

All tunable settings are at the top of `main.py`:

```python
llm_provider = "openai"              # "openai" | "openrouter"
llm_model    = "gpt-4o"

benchmark_path      = "benchmark"
hls_setup_command   = "module load xilinx/vivado-2022.1"
fpga_part           = "xcu280-fsvh2892-2L-e"
clock_period        = "3.33"         # ns
max_task_opt_retries = 3
```

> **Note:** `vitis_hls` is the HLS compiler binary bundled inside the Vivado 2022.1 installation. It is what actually runs C simulation and C synthesis — it is not the same as `vivado`, which is the RTL place-and-route tool.
