from langchain_core.tools import tool
from langchain_core.tools import StructuredTool
from typing import Optional, Type, TypedDict, ClassVar
from pydantic import BaseModel, Field
from langchain_core.callbacks import (
    AsyncCallbackManagerForToolRun,
    CallbackManagerForToolRun,
)
from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage, SystemMessage
from langchain_core.output_parsers import StrOutputParser
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.tools import BaseTool
from langgraph.graph import StateGraph, START, END
from langchain_openrouter import ChatOpenRouter
from dotenv import load_dotenv
import subprocess
from pathlib import Path
from datetime import datetime
import re
import time
from prompts.hw_sw_partition_prompt import SYSTEM_PROMPT, CODE_ANALYSIS_PROMPT
from prompts.task_pipeline_prompt import TASK_PIPELINE_PROMPT, TASK_PIPELINE_STRATEGY_PROMPT_4
from prompts.task_opt_prompt import *

""" enviroment set up """
load_dotenv()
benchmark_path = "benchmark"


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

    chat_model = ChatOpenAI(model='gpt-4o')
    #chat_model = ChatOpenRouter(model='stepfun/step-3.5-flash:free',temperature=0)
    messages = [
        HumanMessage(content=PROMPT_COMPLETE),
    ]
    response = chat_model.invoke(messages)
    print(response)

    response_text = response.content if hasattr(response, "content") else str(response)
    if not isinstance(response_text, str):
        response_text = str(response_text)

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
        self, application: str, run_manager: Optional[CallbackManagerForToolRun] = None
    ) -> str:
        report_content = generate_gprof_report(application)
        analysis_report(application, report_content)


class GraphState(TypedDict):
    application: str
    report_content: str
    analysis_result: str
    pipeline_result: str
    task_opt_result: str


def generate_report_node(state: GraphState) -> GraphState:
    report_content = generate_gprof_report(state["application"])
    return {**state, "report_content": report_content}


def analysis_node(state: GraphState) -> GraphState:
    analysis_result = analysis_report(state["application"], state["report_content"])
    return {**state, "analysis_result": analysis_result}


def task_pipeline_node(state: GraphState) -> GraphState:
    app = state["application"]
    func_description = f"{app} algorithm"

    _SYSTEM_PROMPT = SYSTEM_PROMPT.replace("{ALGO_NAME}", app)
    _SYSTEM_PROMPT = _SYSTEM_PROMPT.replace("{FUNCTION_DESCRIPTION}", func_description)

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

    chat_model = ChatOpenAI(model='gpt-4o')
    #chat_model = ChatOpenRouter(model='stepfun/step-3.5-flash:free', temperature=0)
    chat_completion = chat_model.invoke([HumanMessage(content=prompt_complete)])

    model_name = "gpt4o"
    cur_time = time.strftime('%y%m%d_%H%M', time.localtime())

    pipeline_dir = Path("pipeline") / model_name / app
    pipeline_dir.mkdir(parents=True, exist_ok=True)

    chat_file_path = pipeline_dir / f"pipeline_{model_name}_{app}_{cur_time}.txt"
    code_file_path = pipeline_dir / f"pipeline_{model_name}_{app}_{cur_time}.cpp"

    chat_text = chat_completion.content if hasattr(chat_completion, "content") else str(chat_completion)
    if not isinstance(chat_text, str):
        chat_text = str(chat_text)
    with open(chat_file_path, 'w') as chat_file:
        chat_file.write(chat_text)
        chat_file.write("\n\n====================================\n\n")
        chat_file.write(prompt_complete)

    content = chat_text
    match = re.search(r"\`\`\`(.*?)\`\`\`", content, re.DOTALL)
    if match:
        extracted_code = match.group(1).lstrip()
        first_line, _, rest = extracted_code.partition("\n")
        if first_line.strip().lower() in {"cpp", "c++", "c", "cc", "hpp", "h"}:
            extracted_code = rest
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


def _save_stage_opt_output(completion_type: str, prompt_content: str, response_text: str, model_name: str, algo_name: str):
    model_tag = "gpt4" if model_name in ["gpt-4-1106-preview", "gpt-4"] else "gpt3.5"
    cur_time = time.strftime('%y%m%d_%H%M', time.localtime())
    stage_opt_dir = Path("stage_opt") / model_tag / algo_name
    stage_opt_dir.mkdir(parents=True, exist_ok=True)

    chat_file_path = stage_opt_dir / f"{completion_type}_{model_tag}_{cur_time}.txt"
    chat_file_path.write_text(response_text + "\n\n====================================\n\n" + prompt_content, encoding="utf-8")

    if completion_type == "opt_apply":
        code_file_path = stage_opt_dir / f"{completion_type}_{model_tag}_{cur_time}.cpp"
        match = re.search(r"\`\`\`(.*?)\`\`\`", response_text, re.DOTALL)
        if match:
            extracted_code = match.group(1).lstrip()
            first_line, _, rest = extracted_code.partition("\n")
            if first_line.strip().lower() in {"cpp", "c++", "c", "cc", "hpp", "h"}:
                extracted_code = rest
            code_file_path.write_text(extracted_code, encoding="utf-8")


def _parse_opt_list(raw_text: str):
    match = re.search(r"\[(.*?)\]", raw_text, re.DOTALL)
    if not match:
        return []
    raw_list = match.group(1).strip()
    if not raw_list:
        return []
    return [item.strip() for item in raw_list.split(",") if item.strip()]


def _gen_stage_opt_prompt(stage_code: str) -> str:
    pragma_description = ""
    for opt in task_opt_options:
        prompt_name = f"{opt}_PROMPT"
        pragma_description += globals()[prompt_name]
        pragma_description += "-----------------------"
    prompt = OPT_CHOICE_PROMPT.replace("{PRAGMA_DESCRIPTION}", pragma_description)
    prompt = prompt.replace("{STAGE_CODE_CONTENT}", stage_code)
    return prompt


def _apply_opt(stage_code: str, stage_opt_list, algo_name: str, func_description: str, model_name: str):
    _SYSTEM_PROMPT = SYSTEM_PROMPT.replace("{ALGO_NAME}", algo_name)
    _SYSTEM_PROMPT = _SYSTEM_PROMPT.replace("{FUNCTION_DESCRIPTION}", func_description)

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

    full_prompt = _SYSTEM_PROMPT + apply_prompt
    chat_model = ChatOpenAI(model='gpt-4o')
    #chat_model = ChatOpenRouter(model='stepfun/step-3.5-flash:free', temperature=0)
    response = chat_model.invoke([HumanMessage(content=full_prompt)])
    response_text = response.content if hasattr(response, "content") else str(response)
    if not isinstance(response_text, str):
        response_text = str(response_text)
    _save_stage_opt_output("opt_apply", full_prompt, response_text, model_name, algo_name)
    return response_text


def task_opt_node(state: GraphState) -> GraphState:
    algo_name = state["application"]
    func_description = f"{algo_name}"
    model_name = "gpt-4o"

    stage_code = _get_latest_pipeline_cpp(algo_name)
    if not stage_code:
        code_path = Path(f"{benchmark_path}/{algo_name}/{algo_name}.cpp")
        if code_path.exists():
            stage_code = code_path.read_text(encoding="utf-8")
        else:
            stage_code = ""

    choose_prompt = _gen_stage_opt_prompt(stage_code)
    chat_model = ChatOpenAI(model='gpt-4o')
    #chat_model = ChatOpenRouter(model='stepfun/step-3.5-flash:free', temperature=0)
    choose_response = chat_model.invoke([HumanMessage(content=choose_prompt)])
    choose_text = choose_response.content if hasattr(choose_response, "content") else str(choose_response)
    if not isinstance(choose_text, str):
        choose_text = str(choose_text)
    _save_stage_opt_output("opt_choose", choose_prompt, choose_text, model_name, algo_name)

    stage_opt_list = _parse_opt_list(choose_text)
    apply_text = _apply_opt(stage_code, stage_opt_list, algo_name, func_description, model_name)
    return {**state, "task_opt_result": apply_text}


graph = StateGraph(GraphState)
graph.add_node("generate_report", generate_report_node)
graph.add_node("analysis", analysis_node)
graph.add_node("task_pipeline", task_pipeline_node)
graph.add_node("task_opt", task_opt_node)
graph.add_edge(START, "generate_report")
graph.add_edge("generate_report", "analysis")
graph.add_edge("analysis", "task_pipeline")
graph.add_edge("task_pipeline", "task_opt")
graph.add_edge("task_opt", END)
app = graph.compile()


if __name__ == "__main__":
    mytool = AutoAnalysisTool()
    print(mytool.name)
    print(mytool.description)
    print(mytool.args)
    print(mytool.return_direct)

    inputs: GraphState = {
        "application": "fir",
        "report_content": "",
        "analysis_result": "",
        "pipeline_result": "",
        "task_opt_result": "",
    }
    result = app.invoke(inputs)
    print(result["analysis_result"])
