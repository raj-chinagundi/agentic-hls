from typing import Optional, Type, TypedDict, ClassVar
from pydantic import BaseModel, Field
from langchain_core.callbacks import CallbackManagerForToolRun
from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage
from langchain_core.tools import BaseTool
from langgraph.graph import StateGraph, START, END
from dotenv import load_dotenv
import subprocess
from pathlib import Path
from datetime import datetime
import re
import time
import shlex
import xml.etree.ElementTree as ET
from prompts.hw_sw_partition_prompt import SYSTEM_PROMPT, CODE_ANALYSIS_PROMPT
from prompts.task_pipeline_prompt import TASK_PIPELINE_PROMPT, TASK_PIPELINE_STRATEGY_PROMPT_4
from prompts.task_opt_prompt import *

""" environment set up """
load_dotenv()
benchmark_path = "benchmark"
hls_setup_command = "module load xilinx/vitis-2022.1 && /data/sse/fpga/amd/scripts/fpga_env.sh"
fpga_part = "xcu280-fsvh2892-2L-e"
clock_period = "3.33"
max_task_opt_retries = 3

# LLM config — change provider and model here, nothing else needs to be touched
llm_provider = "openai"   # "openai" | "openrouter"
llm_model = "gpt-4o"
llm_model_safe = re.sub(r'[/:\\"\' ]', "_", llm_model)


def _get_chat_model():
    if llm_provider == "openrouter":
        from langchain_openrouter import ChatOpenRouter  # type: ignore[import-untyped]
        return ChatOpenRouter(model=llm_model, temperature=0)
    return ChatOpenAI(model=llm_model)


def _normalize_response_text(response):
    response_text = response.content if hasattr(response, "content") else str(response)
    if not isinstance(response_text, str):
        response_text = str(response_text)
    return response_text


def _strip_markdown_language_tag(extracted_code: str) -> str:
    extracted_code = extracted_code.lstrip()
    first_line, _, rest = extracted_code.partition("\n")
    if first_line.strip().lower() in {"cpp", "c++", "c", "cc", "hpp", "h"}:
        return rest
    return extracted_code


def _get_or_create_run_timestamp(state: dict) -> str:
    if state.get("run_timestamp"):
        return state["run_timestamp"]
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def generate_gprof_report(app, output_file='gprof.txt'):
    compile_command = f"g++ -pg -o {benchmark_path}/{app}/{app} {benchmark_path}/{app}/{app}.cpp"
    subprocess.run(compile_command, shell=True, check=True)

    run_command = f"./{benchmark_path}/{app}/{app}"
    subprocess.run(run_command, shell=True, check=True)

    gprof_command = f"gprof {benchmark_path}/{app}/{app} gmon.out > {benchmark_path}/{app}/{output_file}"
    subprocess.run(gprof_command, shell=True, check=True)

    report_path = benchmark_path + '/' + app + '/' + output_file
    with open(report_path, 'r') as file:
        report = file.read()

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    profiling_dir = Path("profiling_report") / app
    profiling_dir.mkdir(parents=True, exist_ok=True)
    profiling_path = profiling_dir / f"{app}_{timestamp}.cpp"
    profiling_path.write_text(report, encoding="utf-8")

    return report


def analysis_report(app, report_content):
    PROMPT_COMPLETE = ""
    PROMPT_COMPLETE += SYSTEM_PROMPT.replace("{ALGO_NAME}", app)

    app_file_path = benchmark_path + '/' + app + '/' + app + '.cpp'
    with open(app_file_path, 'r') as app_file:
        code_content = app_file.read()

    _CODE_ANALYSIS_PROMPT = CODE_ANALYSIS_PROMPT.replace("{CODE_CONTENT}", code_content)
    _CODE_ANALYSIS_PROMPT = _CODE_ANALYSIS_PROMPT.replace("{REPORT_CONTENT}", report_content)

    PROMPT_COMPLETE += _CODE_ANALYSIS_PROMPT
    print(PROMPT_COMPLETE)

    chat_model = _get_chat_model()
    messages = [
        HumanMessage(content=PROMPT_COMPLETE),
    ]
    response = chat_model.invoke(messages)
    print(response)

    response_text = _normalize_response_text(response)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    bottleneck_dir = Path("bottleneck_report") / app
    bottleneck_dir.mkdir(parents=True, exist_ok=True)
    bottleneck_path = bottleneck_dir / f"{app}_{timestamp}.txt"
    bottleneck_path.write_text(response_text, encoding="utf-8")

    return response_text


class AutoAnalysisInput(BaseModel):
    application: str = Field(description="application name")


class AutoAnalysisTool(BaseTool):
    name: ClassVar[str] = "Auto Code Analysis Tool"
    description: ClassVar[str] = "Used to call the gprof tool to generate performance reports, analyze the reports using LLM, and output various performance indicators"
    args_schema: ClassVar[Type[BaseModel]] = AutoAnalysisInput
    return_direct: ClassVar[bool] = True

    def _run(
        self, application: str, run_manager: Optional[CallbackManagerForToolRun] = None  # noqa: ARG002
    ) -> str:
        report_content = generate_gprof_report(application)
        return analysis_report(application, report_content)


class GraphState(TypedDict):
    application: str
    top_function: str
    report_content: str
    analysis_result: str
    pipeline_result: str
    task_opt_result: str
    task_opt_code_path: str
    task_opt_retry_count: int
    csim_status: str
    csim_log: str
    csim_log_path: str
    csynth_status: str
    csynth_log_path: str
    csynth_report_path: str
    results_path: str
    results_summary: str
    run_timestamp: str


def generate_report_node(state: GraphState) -> GraphState:
    report_content = generate_gprof_report(state["application"])
    return {**state, "report_content": report_content}


def analysis_node(state: GraphState) -> GraphState:
    analysis_result = analysis_report(state["application"], state["report_content"])
    return {**state, "analysis_result": analysis_result}


def task_pipeline_node(state: GraphState) -> GraphState:
    app = state["application"]

    _SYSTEM_PROMPT = SYSTEM_PROMPT.replace("{ALGO_NAME}", app)

    code_path = f"{benchmark_path}/{app}/{app}.cpp"
    with open(code_path, "r") as code_file:
        code_content = "\n" + code_file.read()

    bottleneck_content = ""
    bottleneck_dir = Path("bottleneck_report") / app
    if bottleneck_dir.exists():
        bottleneck_files = sorted(
            bottleneck_dir.glob("*.txt"),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )
        if bottleneck_files:
            with open(bottleneck_files[0], "r") as bottleneck_file:
                bottleneck_content = bottleneck_file.read()

    prompt_complete = _SYSTEM_PROMPT + TASK_PIPELINE_PROMPT + code_content
    if bottleneck_content:
        prompt_complete += "\n\nBottleneck analysis report:\n" + bottleneck_content
    prompt_complete += TASK_PIPELINE_STRATEGY_PROMPT_4

    chat_model = _get_chat_model()
    chat_completion = chat_model.invoke([HumanMessage(content=prompt_complete)])

    cur_time = time.strftime('%y%m%d_%H%M', time.localtime())

    pipeline_dir = Path("pipeline") / llm_model_safe / app
    pipeline_dir.mkdir(parents=True, exist_ok=True)

    chat_file_path = pipeline_dir / f"pipeline_{llm_model_safe}_{app}_{cur_time}.txt"
    code_file_path = pipeline_dir / f"pipeline_{llm_model_safe}_{app}_{cur_time}.cpp"

    chat_text = _normalize_response_text(chat_completion)
    with open(chat_file_path, 'w') as chat_file:
        chat_file.write(chat_text)
        chat_file.write("\n\n====================================\n\n")
        chat_file.write(prompt_complete)

    match = re.search(r"\`\`\`(.*?)\`\`\`", chat_text, re.DOTALL)
    if match:
        extracted_code = _strip_markdown_language_tag(match.group(1))
        with open(code_file_path, 'w') as code_file:
            code_file.write(extracted_code)

    return {**state, "pipeline_result": chat_text}


task_opt_options = [
    "ALLOCATION", "RESOURCE", "INLINE",
    "FUNCTION_INSTANTIATE", "STREAM", "PIPELINE",
    "OCCURRENCE", "UNROLL", "DEPENDENCE",
    "LOOP_FLATTEN", "LOOP_MERGE", "LOOP_TRIPCOUNT",
    "ARRAY_MAP", "ARRAY_PARTITION", "ARRAY_RESHAPE",
    "DATA_PACK",
]


def _get_latest_pipeline_cpp(app_name: str) -> str:
    pipeline_root = Path("pipeline")
    if not pipeline_root.exists():
        return ""
    candidates = list(pipeline_root.glob(f"*/{app_name}/*.cpp"))
    if not candidates:
        return ""
    latest = max(candidates, key=lambda p: p.stat().st_mtime)
    return latest.read_text(encoding="utf-8")


def _get_latest_stage_opt_cpp_path(app_name: str) -> str:
    stage_opt_root = Path("stage_opt")
    if not stage_opt_root.exists():
        return ""
    candidates = list(stage_opt_root.glob(f"*/{app_name}/opt_apply_*.cpp"))
    if not candidates:
        return ""
    latest = max(candidates, key=lambda p: p.stat().st_mtime)
    return str(latest)


def _save_stage_opt_output(completion_type: str, prompt_content: str, response_text: str, algo_name: str):
    cur_time = time.strftime('%y%m%d_%H%M', time.localtime())
    stage_opt_dir = Path("stage_opt") / llm_model_safe / algo_name
    stage_opt_dir.mkdir(parents=True, exist_ok=True)

    chat_file_path = stage_opt_dir / f"{completion_type}_{llm_model_safe}_{cur_time}.txt"
    chat_file_path.write_text(response_text + "\n\n====================================\n\n" + prompt_content, encoding="utf-8")

    code_path = ""
    if completion_type == "opt_apply":
        code_file_path = stage_opt_dir / f"{completion_type}_{llm_model_safe}_{cur_time}.cpp"
        match = re.search(r"\`\`\`(.*?)\`\`\`", response_text, re.DOTALL)
        if match:
            extracted_code = _strip_markdown_language_tag(match.group(1))
            code_file_path.write_text(extracted_code, encoding="utf-8")
            code_path = str(code_file_path)
    return code_path


def _parse_opt_list(raw_text: str):
    match = re.search(r"\[(.*?)\]", raw_text, re.DOTALL)
    if not match:
        return []
    raw_list = match.group(1).strip()
    if not raw_list:
        return []
    return [item.strip().strip("'\"") for item in raw_list.split(",") if item.strip()]


def _gen_stage_opt_prompt(stage_code: str, csim_error_log: str = "") -> str:
    pragma_description = ""
    for opt in task_opt_options:
        prompt_name = f"{opt}_PROMPT"
        pragma_description += globals()[prompt_name]
        pragma_description += "-----------------------"
    prompt = OPT_CHOICE_PROMPT.replace("{PRAGMA_DESCRIPTION}", pragma_description)
    prompt = prompt.replace("{STAGE_CODE_CONTENT}", stage_code)
    if csim_error_log:
        prompt += "\n\nThe exact code above failed HLS csim. Use the following csim error log to choose pragmas/fixes that preserve functionality:\n"
        prompt += csim_error_log
    return prompt


def _apply_opt(stage_code: str, stage_opt_list, algo_name: str, csim_error_log: str = ""):
    _SYSTEM_PROMPT = SYSTEM_PROMPT.replace("{ALGO_NAME}", algo_name)

    opt_list_text = ""
    pragma_demo_complete = ""
    for i, opt_option in enumerate(stage_opt_list):
        opt_list_text += str(i) + ". " + opt_option + "\n"
        pragma_name = opt_option.split(" ")[-1].upper() + "_DEMO"
        if pragma_name in globals():
            pragma_demo_complete += str(i) + ". " + opt_option + ":\n" + globals()[pragma_name] + "\n"

    apply_prompt = APPLY_OPT_PROMPT.replace("{STAGE_CODE_CONTENT}", stage_code)
    apply_prompt = apply_prompt.replace("{OPT_LIST}", opt_list_text)
    apply_prompt = apply_prompt.replace("{PRAGMA_DEMO}", pragma_demo_complete)
    if csim_error_log:
        apply_prompt += "\n\nThe exact code above failed HLS csim. Fix the code using the following csim error log while preserving the optimization intent:\n"
        apply_prompt += csim_error_log

    full_prompt = _SYSTEM_PROMPT + apply_prompt
    chat_model = _get_chat_model()
    response = chat_model.invoke([HumanMessage(content=full_prompt)])
    response_text = _normalize_response_text(response)
    code_path = _save_stage_opt_output("opt_apply", full_prompt, response_text, algo_name)
    return response_text, code_path


def task_opt_node(state: GraphState) -> GraphState:
    algo_name = state["application"]

    retry_count = state.get("task_opt_retry_count", 0)
    csim_error_log = state.get("csim_log", "") if state.get("csim_status") == "failed" else ""

    if csim_error_log and state.get("task_opt_code_path"):
        stage_code = Path(state["task_opt_code_path"]).read_text(encoding="utf-8")
        retry_count += 1
    else:
        stage_code = _get_latest_pipeline_cpp(algo_name)
        if not stage_code:
            code_path = Path(f"{benchmark_path}/{algo_name}/{algo_name}.cpp")
            if code_path.exists():
                stage_code = code_path.read_text(encoding="utf-8")
            else:
                stage_code = ""

    choose_prompt = _gen_stage_opt_prompt(stage_code, csim_error_log)
    chat_model = _get_chat_model()
    choose_response = chat_model.invoke([HumanMessage(content=choose_prompt)])
    choose_text = _normalize_response_text(choose_response)
    _save_stage_opt_output("opt_choose", choose_prompt, choose_text, algo_name)

    stage_opt_list = _parse_opt_list(choose_text)
    apply_text, code_path = _apply_opt(stage_code, stage_opt_list, algo_name, csim_error_log)
    if not code_path:
        code_path = _get_latest_stage_opt_cpp_path(algo_name)

    return {
        **state,
        "task_opt_result": apply_text,
        "task_opt_code_path": code_path,
        "task_opt_retry_count": retry_count,
        "csim_status": "",
        "csim_log": "",
        "csim_log_path": "",
    }


def _write_hls_tcl(mode: str, tcl_path: Path, project_path: Path, source_cpp: str, include_dir: str, top_function: str, tb_cpp: str = ""):
    lines = [
        f'open_project -reset {project_path.as_posix()}',
        f'set_top {top_function}',
        f'add_files {Path(source_cpp).as_posix()} -cflags "-I{Path(include_dir).as_posix()}"',
    ]
    if mode == "csim" and tb_cpp:
        lines.append(f'add_files -tb {Path(tb_cpp).as_posix()} -cflags "-I{Path(include_dir).as_posix()}"')
    lines += [
        'open_solution -reset solution1',
        f'set_part {{{fpga_part}}}',
        f'create_clock -period {clock_period} -name default',
    ]
    if mode == "csim":
        lines.append('csim_design')
    else:
        lines.append('csynth_design')
    lines.append('exit')
    tcl_path.write_text("\n".join(lines), encoding="utf-8")


def _run_hls(tcl_path: Path):
    command = f'{hls_setup_command} && vitis_hls -f {shlex.quote(str(tcl_path))}'
    return subprocess.run(
        ["bash", "-lc", command],
        capture_output=True,
        text=True,
    )


def csim_node(state: GraphState) -> GraphState:
    run_timestamp = _get_or_create_run_timestamp(state)
    app_name = state["application"]
    top_function = state["top_function"]
    source_cpp = state.get("task_opt_code_path") or _get_latest_stage_opt_cpp_path(app_name)

    # Only use a dedicated test bench file (e.g. fir_tb.cpp) — never the original
    # benchmark source, which defines the same top function and would cause a
    # duplicate symbol linker error during csim compilation.
    dedicated_tb = Path(f"{benchmark_path}/{app_name}/{app_name}_tb.cpp")
    tb_cpp = str(dedicated_tb) if dedicated_tb.exists() else ""

    run_dir = Path("hls_runs") / app_name / run_timestamp / f"csim_retry_{state.get('task_opt_retry_count', 0)}"
    run_dir.mkdir(parents=True, exist_ok=True)
    tcl_path = run_dir / "csim.tcl"
    log_path = run_dir / "csim.log"
    project_path = run_dir / "project"

    _write_hls_tcl("csim", tcl_path, project_path, source_cpp, f"{benchmark_path}/{app_name}", top_function, tb_cpp)
    result = _run_hls(tcl_path)
    log_text = (result.stdout or "") + ("\n" + result.stderr if result.stderr else "")
    log_path.write_text(log_text, encoding="utf-8")

    status = "passed" if result.returncode == 0 else "failed"
    return {
        **state,
        "run_timestamp": run_timestamp,
        "csim_status": status,
        "csim_log": log_text,
        "csim_log_path": str(log_path),
    }


def _route_after_csim(state: GraphState):
    if state.get("csim_status") == "passed":
        return "csynth"
    if state.get("task_opt_retry_count", 0) < max_task_opt_retries:
        return "retry_task_opt"
    return "end"


def csynth_node(state: GraphState) -> GraphState:
    run_timestamp = _get_or_create_run_timestamp(state)
    app_name = state["application"]
    top_function = state["top_function"]
    source_cpp = state.get("task_opt_code_path") or _get_latest_stage_opt_cpp_path(app_name)

    run_dir = Path("hls_runs") / app_name / run_timestamp / "csynth"
    run_dir.mkdir(parents=True, exist_ok=True)
    tcl_path = run_dir / "csynth.tcl"
    log_path = run_dir / "csynth.log"
    project_path = run_dir / "project"

    _write_hls_tcl("csynth", tcl_path, project_path, source_cpp, f"{benchmark_path}/{app_name}", top_function)
    result = _run_hls(tcl_path)
    log_text = (result.stdout or "") + ("\n" + result.stderr if result.stderr else "")
    log_path.write_text(log_text, encoding="utf-8")

    report_candidates = list((project_path / "solution1" / "syn" / "report").glob("*_csynth.xml"))
    report_path = str(report_candidates[0]) if report_candidates else ""
    status = "passed" if result.returncode == 0 and report_path else "failed"
    return {
        **state,
        "run_timestamp": run_timestamp,
        "csynth_status": status,
        "csynth_log_path": str(log_path),
        "csynth_report_path": report_path,
    }


def _xml_tag_text(root, tag_name: str) -> str:
    for elem in root.iter():
        if elem.tag.endswith(tag_name) and elem.text:
            return elem.text.strip()
    return ""


def collect_results_node(state: GraphState) -> GraphState:
    app_name = state["application"]
    run_timestamp = _get_or_create_run_timestamp(state)
    results_dir = Path("results") / app_name
    results_dir.mkdir(parents=True, exist_ok=True)
    results_path = results_dir / f"{app_name}_res_{run_timestamp}.txt"

    summary_lines = [
        f"application: {app_name}",
        f"top_function: {state['top_function']}",
        f"run_timestamp: {run_timestamp}",
        f"csim_status: {state.get('csim_status', '')}",
        f"csynth_status: {state.get('csynth_status', '')}",
    ]

    report_path = state.get("csynth_report_path", "")
    if report_path and Path(report_path).exists():
        root = ET.parse(report_path).getroot()
        summary_lines.extend([
            f"estimated_clock_period: {_xml_tag_text(root, 'EstimatedClockPeriod')}",
            f"best_latency: {_xml_tag_text(root, 'Best-caseLatency')}",
            f"worst_latency: {_xml_tag_text(root, 'Worst-caseLatency')}",
            f"interval_min: {_xml_tag_text(root, 'Interval-min')}",
            f"interval_max: {_xml_tag_text(root, 'Interval-max')}",
            f"lut: {_xml_tag_text(root, 'LUT')}",
            f"ff: {_xml_tag_text(root, 'FF')}",
            f"bram_18k: {_xml_tag_text(root, 'BRAM_18K')}",
            f"dsp: {_xml_tag_text(root, 'DSP')}",
            f"report_path: {report_path}",
        ])
    else:
        summary_lines.append("report_path: ")

    results_summary = "\n".join(summary_lines)
    results_path.write_text(results_summary, encoding="utf-8")
    return {
        **state,
        "results_path": str(results_path),
        "results_summary": results_summary,
    }


graph = StateGraph(GraphState)
graph.add_node("generate_report", generate_report_node)
graph.add_node("analysis", analysis_node)
graph.add_node("task_pipeline", task_pipeline_node)
graph.add_node("task_opt", task_opt_node)
graph.add_node("csim", csim_node)
graph.add_node("csynth", csynth_node)
graph.add_node("collect_results", collect_results_node)
graph.add_edge(START, "generate_report")
graph.add_edge("generate_report", "analysis")
graph.add_edge("analysis", "task_pipeline")
graph.add_edge("task_pipeline", "task_opt")
graph.add_edge("task_opt", "csim")
graph.add_conditional_edges(
    "csim",
    _route_after_csim,
    {
        "retry_task_opt": "task_opt",
        "csynth": "csynth",
        "end": END,
    },
)
graph.add_edge("csynth", "collect_results")
graph.add_edge("collect_results", END)
app = graph.compile()


if __name__ == "__main__":
    mytool = AutoAnalysisTool()
    print(mytool.name)
    print(mytool.description)
    print(mytool.args)
    print(mytool.return_direct)

    top_function = input("Enter top function name: ").strip()
    if not top_function:
        raise ValueError("Top function name is required")

    inputs: GraphState = {
        "application": "fir",
        "top_function": top_function,
        "report_content": "",
        "analysis_result": "",
        "pipeline_result": "",
        "task_opt_result": "",
        "task_opt_code_path": "",
        "task_opt_retry_count": 0,
        "csim_status": "",
        "csim_log": "",
        "csim_log_path": "",
        "csynth_status": "",
        "csynth_log_path": "",
        "csynth_report_path": "",
        "results_path": "",
        "results_summary": "",
        "run_timestamp": "",
    }
    result = app.invoke(inputs)
    if result.get("results_summary"):
        print(result["results_summary"])
    elif result.get("csim_log"):
        print(result["csim_log"])
