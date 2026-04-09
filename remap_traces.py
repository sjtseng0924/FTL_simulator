import argparse
from pathlib import Path


def read_logical_sectors(config_path: Path) -> int:
    values: dict[str, int] = {}
    with config_path.open("r", encoding="utf-8") as fh:
        for line in fh:
            parts = line.strip().split()
            if len(parts) == 2:
                values[parts[0]] = int(parts[1])

    logical_bytes = values["LsizeByte"]
    sector_bytes = values["sectorSizeByte"]
    return logical_bytes // sector_bytes


def wrap_trace(
    input_path: Path,
    output_path: Path,
    logical_sectors: int,
    limit: int | None,
) -> tuple[int, int, int, int]:
    total = 0
    kept = 0
    reads = 0
    writes = 0

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with (
        input_path.open("r", encoding="utf-8") as in_fh,
        output_path.open("w", encoding="utf-8") as out_fh,
    ):
        for line in in_fh:
            if limit is not None and total >= limit:
                break

            parts = line.split()
            if len(parts) != 4:
                continue

            _, op, sector_text, length_text = parts
            if op not in ("R", "W"):
                continue

            sector = int(sector_text)
            length = int(length_text)
            total += 1

            if length <= 0 or length > logical_sectors:
                continue

            wrapped_sector = sector % (logical_sectors - length + 1)
            out_fh.write(f"0 {op} {wrapped_sector} {length}\n")
            kept += 1
            if op == "W":
                writes += 1
            else:
                reads += 1

    return total, kept, reads, writes


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Wrap raw trace sectors into the simulator logical space."
    )
    parser.add_argument(
        "inputs",
        nargs="*",
        default=[
            "rawfile952G_simple_LUN3.txt",
            "rawfile952G_simple_LUN4.txt",
            "rawfile952G_simple_LUN6.txt",
        ],
        help="raw trace files in 'tag op sector length' format",
    )
    parser.add_argument("--config", default="files/config.txt")
    parser.add_argument("--output-dir", default="remapped_traces")
    parser.add_argument("--limit", type=int, default=None)
    args = parser.parse_args()

    logical_sectors = read_logical_sectors(Path(args.config))
    print(f"logical sectors: {logical_sectors}")

    for input_name in args.inputs:
        input_path = Path(input_name)
        output_path = Path(args.output_dir) / f"{input_path.stem}_wrap.txt"
        total, kept, reads, writes = wrap_trace(
            input_path=input_path,
            output_path=output_path,
            logical_sectors=logical_sectors,
            limit=args.limit,
        )

        ratio = kept / total if total else 0.0
        print(f"{input_path.name} -> {output_path}")
        print(f"  total : {total}")
        print(f"  kept  : {kept} ({ratio:.4%})")
        print(f"  read/write: {reads} / {writes}")


if __name__ == "__main__":
    main()
