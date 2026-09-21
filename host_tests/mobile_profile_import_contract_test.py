from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

def read(name):
    return (root / name).read_text(encoding="utf-8")

pio = read("platformio.ini")
cmake = read("main/CMakeLists.txt")
manifest = read("main/idf_component.yml")
portal_h = read("MapWebEditor.h")
portal = read("MapWebEditor.cpp")
validator = read("DocumentImportValidator.cpp")
analysis = read("DocumentAnalysisScreen.cpp")
prepared = read("PreparedProfileService.cpp")
extra = read("ExtraFeaturesScreen.cpp")

checks = {
    "core build environment": "default_envs = jc1060p4-core" in pio,
    "mobile import build flag": "MG_MOBILE_PROFILE_IMPORT=1" in pio and "MG_MOBILE_PROFILE_IMPORT=1" in cmake,
    "no AI runtime dependency": "esp-dl" not in manifest.lower() and "pp_ocr" not in manifest.lower(),
    "no generated-model prebuild": "pio_ppocr_generated_source_fix.py" not in pio,
    "JSON-only upload kind": "Json" in portal_h and "Jpeg" not in portal_h and "Pdf" not in portal_h,
    "single prepared profile": "kMaxDocumentItems = 1U" in portal_h,
    "256 KiB ceiling": "256U * 1024U" in portal,
    "companion API session": '"/api/profile/session"' in portal and "handleProfileApiSession" in portal,
    "companion API upload": '"/api/profile/upload"' in portal,
    "companion API chunk": '"/api/profile/chunk"' in portal,
    "valid token getter": "documentApToken(" not in portal and "documentSessionToken()" in portal,
    "JSON envelope validation": "Prepared profile is not a JSON object" in validator,
    "canonical parser": "CanonicalWorkplaceJsonAdapter" in (analysis + prepared),
    "electrical contract validation": "ProductionProfileContract::buildRuntimeProfile" in (analysis + prepared),
    "mobile profile menu": '"MOBİL PROFİL"' in extra and '"MOBILE PROFILE"' in extra,
}

retired_files = [
    "DocumentOcrBackend.cpp", "DocumentOcrGeometry.cpp", "DocumentSemanticAnalyzer.cpp",
    "DocumentSchematicTracer.cpp", "DocumentImageInkFilter.cpp", "DocumentAnalysisPipeline.cpp",
    "DocumentNetReviewScreen.cpp", "ocr_full_target", "ocr_idf_target",
    "tools/pio_ppocr_generated_source_fix.py",
]
checks["retired runtime removed"] = all(not (root / name).exists() for name in retired_files)

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("PASS" if ok else "FAIL") + ": " + name)
if failed:
    raise SystemExit("mobile profile import contract failed: " + ", ".join(failed))
print(f"mobile_profile_import_contract_test: PASS ({len(checks)} checks)")
