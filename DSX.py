import time
import subprocess

def is_dsy_running():
    # Run the tasklist command and check for "DualSenseY.exe" in the output
    result = subprocess.run(['tasklist'], stdout=subprocess.PIPE, text=True)
    return 'DualSenseY.exe' in result.stdout

def main():
    while True:
        time.sleep(5)  # Wait for 5 seconds
        if not is_dsy_running():
            break

if __name__ == "__main__":
    main()
