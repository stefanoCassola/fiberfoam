"""Download and export endpoints for pipeline results."""
import csv
import io
import os
import tarfile
import tempfile

from fastapi import APIRouter, HTTPException
from fastapi.responses import StreamingResponse

from routes.pipeline import _pipelines

router = APIRouter()


@router.get("/download/{pipeline_id}")
async def download_case(pipeline_id: str):
    """Create a tar.gz archive of the pipeline's case directories and stream it."""
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    case_dirs = state.get("caseDirs", [])
    if not case_dirs:
        raise HTTPException(status_code=404, detail="No case directories found")

    # Verify at least one case dir exists
    existing_dirs = [d for d in case_dirs if os.path.isdir(d)]
    if not existing_dirs:
        raise HTTPException(
            status_code=404, detail="Case directories no longer exist on disk"
        )

    case_name = os.path.basename(existing_dirs[0]).rsplit("_", 1)[0] or pipeline_id
    archive_name = f"{case_name}_{pipeline_id}.tar.gz"

    # Create the tar.gz in a temp file and stream it
    tmp = tempfile.SpooledTemporaryFile(max_size=50 * 1024 * 1024)  # 50 MB spool
    with tarfile.open(fileobj=tmp, mode="w:gz") as tar:
        for d in existing_dirs:
            arcname = os.path.basename(d)
            tar.add(d, arcname=arcname)
    tmp.seek(0)

    return StreamingResponse(
        tmp,
        media_type="application/gzip",
        headers={"Content-Disposition": f'attachment; filename="{archive_name}"'},
    )


@router.get("/csv/{pipeline_id}")
async def results_csv(pipeline_id: str):
    """Return permeability results as a CSV download."""
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    results = state.get("results", {})
    if not results:
        raise HTTPException(
            status_code=404, detail="No results available for this pipeline"
        )

    # Build CSV from results dict
    output = io.StringIO()
    writer = csv.writer(output)

    # Determine columns from first result entry
    if isinstance(results, dict):
        # Flatten dict results into rows
        writer.writerow(["key", "value"])
        for k, v in results.items():
            if isinstance(v, dict):
                for sub_k, sub_v in v.items():
                    writer.writerow([f"{k}.{sub_k}", sub_v])
            elif isinstance(v, list):
                for i, item in enumerate(v):
                    if isinstance(item, dict):
                        for sub_k, sub_v in item.items():
                            writer.writerow([f"{k}[{i}].{sub_k}", sub_v])
                    else:
                        writer.writerow([f"{k}[{i}]", item])
            else:
                writer.writerow([k, v])
    elif isinstance(results, list):
        if results and isinstance(results[0], dict):
            headers = list(results[0].keys())
            writer.writerow(headers)
            for row in results:
                writer.writerow([row.get(h, "") for h in headers])

    csv_content = output.getvalue()
    return StreamingResponse(
        io.BytesIO(csv_content.encode("utf-8")),
        media_type="text/csv",
        headers={
            "Content-Disposition": f'attachment; filename="results_{pipeline_id}.csv"'
        },
    )


@router.get("/files/{pipeline_id}")
async def list_files(pipeline_id: str):
    """Return a list of output files in the pipeline's case directories."""
    state = _pipelines.get(pipeline_id)
    if not state:
        raise HTTPException(status_code=404, detail="Pipeline not found")

    case_dirs = state.get("caseDirs", [])
    files: list[dict] = []

    for case_dir in case_dirs:
        if not os.path.isdir(case_dir):
            continue
        for root, _dirs, filenames in os.walk(case_dir):
            for fname in sorted(filenames):
                full_path = os.path.join(root, fname)
                rel_path = os.path.relpath(full_path, case_dir)
                try:
                    size = os.path.getsize(full_path)
                except OSError:
                    size = 0
                files.append({
                    "caseDir": os.path.basename(case_dir),
                    "path": rel_path,
                    "size": size,
                })

    return {"pipelineId": pipeline_id, "files": files}
