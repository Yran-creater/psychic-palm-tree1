@echo off
pushd "%~dp0backend"
if errorlevel 1 (
    echo ERROR: cannot cd to backend folder
    pause
    exit /b 1
)

pip install -r requirements.txt -q

echo.
echo Server: http://127.0.0.1:8000
echo Press Ctrl+C to stop
echo.

python -m uvicorn main:app --host 127.0.0.1 --port 8000 --reload
popd
