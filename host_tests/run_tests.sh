#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

python3 "$project_dir/host_tests/beta_1_0_baseline_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/mcp23s17_fix1_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/mobile_profile_import_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/ble_profile_stage03_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/ble_profile_stage03_fix1_stack_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/main_menu_language_touch_contract_test.py"
python3 "$project_dir/host_tests/hardware_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/normal_map_editor_contract_test.py"
python3 "$project_dir/host_tests/stage03_fix3_graph_labels_ascii_arrow_contract_test.py"
python3 "$project_dir/host_tests/stage03_fix5_operator_workflow_contract_test.py"
python3 "$project_dir/host_tests/stage03_fix6_optional_map_edit_contract_test.py"
python3 "$project_dir/host_tests/stage03_fix7_edit_audit_report_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/stage03_fix8_wifi_ondemand_localization_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/stage03_fix9_report_header_simplify_contract_test.py"
python3 "$project_dir/host_tests/stage03_fix10_physical_fault_summary_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/stage03_fix11_final_reverse_point_display_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/scan_header_subd_ds_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/single_step_hold_contract_test.py"

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir" \
  "$project_dir/CableMap.cpp" \
  "$project_dir/CableNetGraph.cpp" \
  "$project_dir/ScanSource.cpp" \
  "$project_dir/Mcp23s17Hal.cpp" \
  "$project_dir/Mcp23s17ScanSource.cpp" \
  "$project_dir/host_tests/mcp23s17_fix1_test.cpp" \
  -o "$build_dir/mcp23s17_fix1_test"
"$build_dir/mcp23s17_fix1_test"

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir" \
  "$project_dir/CableMap.cpp" \
  "$project_dir/CableNetGraph.cpp" \
  "$project_dir/CableProfile.cpp" \
  "$project_dir/ScanSource.cpp" \
  "$project_dir/ScanSession.cpp" \
  "$project_dir/ReportFormatter.cpp" \
  "$project_dir/host_tests/scan_session_test.cpp" \
  -o "$build_dir/scan_session_test"
"$build_dir/scan_session_test"

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir" \
  "$project_dir/P4Localization.cpp" \
  "$project_dir/host_tests/localization_test.cpp" \
  -o "$build_dir/localization_test"
"$build_dir/localization_test"

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir" \
  "$project_dir/ProductionProfileContract.cpp" \
  "$project_dir/ProductionWorkflowBridge.cpp" \
  "$project_dir/TestWorkflowController.cpp" \
  "$project_dir/WorkflowArchiveCore.cpp" \
  "$project_dir/CableMap.cpp" \
  "$project_dir/CableProfile.cpp" \
  "$project_dir/host_tests/production_identity_batch10_test.cpp" \
  -o "$build_dir/production_identity_batch10_test"
"$build_dir/production_identity_batch10_test"

g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir" \
  "$project_dir/WorkplaceProfileJsonAdapter.cpp" \
  "$project_dir/WorkplaceProfileLookupCore.cpp" \
  "$project_dir/ProductionProfileContract.cpp" \
  "$project_dir/ProductionWorkflowBridge.cpp" \
  "$project_dir/TestWorkflowController.cpp" \
  "$project_dir/WorkflowArchiveCore.cpp" \
  "$project_dir/CableMap.cpp" \
  "$project_dir/CableProfile.cpp" \
  "$project_dir/host_tests/workplace_pr_http_batch12_test.cpp" \
  -o "$build_dir/workplace_profile_json_test"
"$build_dir/workplace_profile_json_test"

python3 "$project_dir/host_tests/production_identity_batch10_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/workplace_pr_http_batch12_contract_test.py" "$project_dir"
python3 "$project_dir/host_tests/unified_result_table_fix4_contract_test.py" "$project_dir"

python3 "$project_dir/host_tests/result_graph_extra31_contract_test.py"
echo "MG Smart Tester Extra31 result-graph/live-scan host tests: PASS"

python3 "$project_dir/host_tests/upload_existing_firmware_test.py"
echo "MG Smart Tester Extra31 Fix1 direct-upload host tests: PASS"
python3 "$project_dir/host_tests/repair_libsodium_component_test.py"
echo "MG Smart Tester targeted libsodium-repair host tests: PASS"
