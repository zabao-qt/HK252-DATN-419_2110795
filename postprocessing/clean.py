# clean_data.py
import os

# -----------------------------
# Config
# -----------------------------
RAW_FILE = "data.txt"
CLEAN_FILE = "data_clean.txt"

def clean_file(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found.")
        return

    valid = 0
    skipped = 0

    with open(input_path, "r") as fin, open(output_path, "w") as fout:
        for line in fin:
            parts = [p.strip() for p in line.strip().split(",")]
            if len(parts) < 5:
                skipped += 1
                continue

            try:
                lat = float(parts[0])
                lon = float(parts[1])

                wlevel_cm = float(parts[2])
                wdepth_cm = float(parts[3])
                ts = int(float(parts[4]))
            except ValueError:
                skipped += 1
                continue

            # Drop invalid readings
            if wdepth_cm <= 0:
                skipped += 1
                continue

            fout.write(f"{lat:.6f}, {lon:.6f}, {wlevel_cm:.1f}, {wdepth_cm:.1f}, {ts}\n")
            valid += 1

    print(f"[CLEAN] Data cleaned successfully. valid={valid}, skipped={skipped}")

if __name__ == "__main__":
    clean_file(RAW_FILE, CLEAN_FILE)