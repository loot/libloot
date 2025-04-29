rmdir /q /s build\coverage

OpenCppCoverage.exe --sources %cd%\src --sources %cd%\include ^
    --export_type binary:build\coverage\unit.cov --working_dir build ^
    -- build\Debug\libloot_internals_tests.exe

OpenCppCoverage.exe --sources %cd%\src --sources %cd%\include ^
    --input_coverage build\coverage\unit.cov ^
    --export_type cobertura:build\coverage\cobertura.xml ^
    --export_type html:build\coverage --working_dir build ^
    -- build\Debug\libloot_tests.exe

build\coverage\index.html
