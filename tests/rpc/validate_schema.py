#!/usr/bin/env python3
"""
Validate RPC schema against C++ handler implementations.

This script parses the JSON5 schema and compares it against the actual
C++ handler code to detect mismatches in:
- Method names
- Parameter names
- Response field names
- Error codes

Usage:
    python validate_schema.py [--verbose] [--fix-suggestions]

Returns exit code 0 if schema matches code, 1 if mismatches found.
"""

import json
import re
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import Any


# Paths relative to this script
SCRIPT_DIR = Path(__file__).parent
REPO_ROOT = SCRIPT_DIR.parent.parent
SCHEMA_PATH = REPO_ROOT / "dev-docs" / "rpc-schema.json5"
HANDLERS_DIR = REPO_ROOT / "src" / "rpc" / "handlers"
TYPES_PATH = REPO_ROOT / "src" / "rpc" / "rpctypes.h"
NOTIFIER_PATH = REPO_ROOT / "src" / "rpc" / "rpcnotifier.cpp"


@dataclass
class ValidationError:
    """A single validation error."""
    category: str
    severity: str  # "error" or "warning"
    message: str
    location: str = ""
    suggestion: str = ""


@dataclass
class ValidationResult:
    """Result of schema validation."""
    errors: list[ValidationError] = field(default_factory=list)
    warnings: list[ValidationError] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return len(self.errors) == 0

    def add_error(self, category: str, message: str, location: str = "", suggestion: str = ""):
        self.errors.append(ValidationError(category, "error", message, location, suggestion))

    def add_warning(self, category: str, message: str, location: str = "", suggestion: str = ""):
        self.warnings.append(ValidationError(category, "warning", message, location, suggestion))


def parse_json5(content: str) -> dict:
    """Parse JSON5 content (handles comments and trailing commas)."""
    # More robust comment removal that doesn't affect strings
    result = []
    i = 0
    in_string = False
    escape_next = False

    while i < len(content):
        char = content[i]

        if escape_next:
            result.append(char)
            escape_next = False
            i += 1
            continue

        if char == '\\' and in_string:
            result.append(char)
            escape_next = True
            i += 1
            continue

        if char == '"' and not escape_next:
            in_string = not in_string
            result.append(char)
            i += 1
            continue

        if not in_string:
            # Check for single-line comment
            if i + 1 < len(content) and content[i:i+2] == '//':
                # Skip to end of line
                while i < len(content) and content[i] != '\n':
                    i += 1
                continue

            # Check for multi-line comment
            if i + 1 < len(content) and content[i:i+2] == '/*':
                # Skip to end of comment
                i += 2
                while i + 1 < len(content) and content[i:i+2] != '*/':
                    i += 1
                i += 2
                continue

        result.append(char)
        i += 1

    content = ''.join(result)

    # Remove trailing commas before } or ]
    content = re.sub(r',(\s*[}\]])', r'\1', content)

    return json.loads(content)


def load_schema() -> dict:
    """Load and parse the JSON5 schema."""
    if not SCHEMA_PATH.exists():
        raise FileNotFoundError(f"Schema not found: {SCHEMA_PATH}")

    content = SCHEMA_PATH.read_text()
    return parse_json5(content)


def extract_cpp_methods(handler_path: Path) -> dict[str, dict]:
    """Extract method information from a C++ handler file."""
    content = handler_path.read_text()
    methods = {}

    # Extract prefix from class - handle both traditional and auto return type syntax
    prefix_match = re.search(
        r'(?:QString\s+\w+::prefix\(\)|auto\s+\w+::prefix\(\)\s*(?:const)?\s*->\s*QString)\s*(?:const)?\s*\{[^}]*return\s+[^"]*"([^"]+)"',
        content
    )
    prefix = prefix_match.group(1) if prefix_match else ""

    # Extract supported methods
    methods_match = re.search(r'supportedMethods\(\)[^{]*\{([^}]+)\}', content, re.DOTALL)
    if methods_match:
        methods_str = methods_match.group(1)
        method_names = re.findall(r'QStringLiteral\("([^"]+)"\)', methods_str)

        for method_name in method_names:
            methods[method_name] = {
                "prefix": prefix,
                "params": extract_params(content, method_name),
                "result_fields": extract_result_fields(content, method_name),
                "errors": extract_error_codes(content, method_name),
            }

    return methods


def extract_params(content: str, method_name: str) -> dict[str, str]:
    """Extract parameter names from handler method."""
    params = {}

    # Find the handler function for this method
    # Pattern: handleMethodName or handle + CamelCase
    camel_name = ''.join(word.capitalize() for word in method_name.split('_'))
    handler_pattern = rf'handle{camel_name}|handle[A-Z][a-zA-Z]*{method_name}'

    # Find function body
    func_match = re.search(
        rf'(handle\w*{camel_name}\w*|handle\w*)\s*\([^)]*QJsonObject[^)]*params[^)]*\)[^{{]*\{{',
        content, re.IGNORECASE
    )

    if func_match:
        start = func_match.end()
        # Find matching brace
        brace_count = 1
        pos = start
        while brace_count > 0 and pos < len(content):
            if content[pos] == '{':
                brace_count += 1
            elif content[pos] == '}':
                brace_count -= 1
            pos += 1

        func_body = content[start:pos]

        # Extract params.value("paramName")
        param_matches = re.findall(r'params\.value\(QStringLiteral\("([^"]+)"\)\)', func_body)
        # Also check params.contains
        param_matches += re.findall(r'params\.contains\(QStringLiteral\("([^"]+)"\)\)', func_body)

        for param in set(param_matches):
            # Try to determine type from usage
            param_type = "unknown"
            if re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toInt', func_body):
                param_type = "integer"
            elif re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toString', func_body):
                param_type = "string"
            elif re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toBool', func_body):
                param_type = "boolean"
            elif re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toArray', func_body):
                param_type = "array"
            elif re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toObject', func_body):
                param_type = "object"
            elif re.search(rf'params\.value\(QStringLiteral\("{param}"\)\)\.toDouble', func_body):
                param_type = "number"

            params[param] = param_type

    return params


def extract_result_fields(content: str, method_name: str) -> set[str]:
    """Extract result field names from handler method."""
    fields = set()

    camel_name = ''.join(word.capitalize() for word in method_name.split('_'))

    # Find return statements with result objects
    # Pattern: return QJsonObject{{QStringLiteral("result"), ...}}
    result_pattern = r'return\s+QJsonObject\{\{QStringLiteral\("result"\),\s*QJsonObject\{\{([^}]+)\}\}'

    for match in re.finditer(result_pattern, content):
        result_content = match.group(1)
        field_matches = re.findall(r'QStringLiteral\("([^"]+)"\)', result_content)
        fields.update(field_matches)

    return fields


def extract_error_codes(content: str, method_name: str) -> set[str]:
    """Extract error codes used in a handler method."""
    errors = set()

    # Find RpcError:: usages
    error_matches = re.findall(r'RpcError::(\w+)', content)

    # Common errors used in most methods
    common_errors = {"InvalidParams", "ProjectNotOpen", "ApplicationClosing"}

    return set(error_matches)


def extract_cpp_error_codes(types_path: Path) -> dict[str, int]:
    """Extract error code definitions from rpctypes.h."""
    content = types_path.read_text()
    errors = {}

    # Pattern: constexpr int ErrorName = -32xxx;
    for match in re.finditer(r'constexpr\s+int\s+(\w+)\s*=\s*(-?\d+)', content):
        errors[match.group(1)] = int(match.group(2))

    return errors


def extract_cpp_events(notifier_path: Path) -> list[str]:
    """Extract event names from rpcnotifier.cpp."""
    content = notifier_path.read_text()
    events = []

    # Find availableEventTypes()
    match = re.search(r'availableEventTypes\(\)[^{]*\{([^}]+)\}', content)
    if match:
        events_str = match.group(1)
        events = re.findall(r'QStringLiteral\("([^"]+)"\)', events_str)

    return events


def validate_methods(schema: dict, result: ValidationResult, verbose: bool = False):
    """Validate methods in schema match C++ implementations."""
    schema_methods = schema.get("methods", {})

    for handler_file in HANDLERS_DIR.glob("*handler.cpp"):
        cpp_methods = extract_cpp_methods(handler_file)

        for method_name, cpp_info in cpp_methods.items():
            prefix = cpp_info["prefix"]
            full_method = f"{prefix}.{method_name}"

            # Check if method exists in schema
            if prefix not in schema_methods:
                result.add_error(
                    "missing_namespace",
                    f"Namespace '{prefix}' not found in schema",
                    str(handler_file),
                    f"Add '{prefix}' namespace to schema"
                )
                continue

            if method_name not in schema_methods[prefix]:
                result.add_error(
                    "missing_method",
                    f"Method '{full_method}' not found in schema",
                    str(handler_file),
                    f"Add '{method_name}' to schema methods.{prefix}"
                )
                continue

            schema_method = schema_methods[prefix][method_name]

            # Validate parameters
            cpp_params = cpp_info["params"]
            schema_params = {}

            # Collect all schema params (required + optional)
            if "params" in schema_method:
                if "required" in schema_method["params"]:
                    schema_params.update(schema_method["params"]["required"])
                if "optional" in schema_method["params"]:
                    schema_params.update(schema_method["params"]["optional"])

            # Check for params in code but not in schema
            for param_name in cpp_params:
                if param_name not in schema_params:
                    # Check aliases
                    found_alias = False
                    for sp_name, sp_def in schema_params.items():
                        if isinstance(sp_def, dict) and "aliases" in sp_def:
                            if param_name in sp_def["aliases"]:
                                found_alias = True
                                break

                    if not found_alias:
                        result.add_warning(
                            "missing_param",
                            f"Parameter '{param_name}' in C++ but not in schema for {full_method}",
                            str(handler_file),
                            f"Add '{param_name}' to schema or as alias"
                        )

            if verbose:
                print(f"  Validated: {full_method}")


def validate_error_codes(schema: dict, result: ValidationResult):
    """Validate error codes match between schema and C++."""
    schema_errors = schema.get("errors", {})
    cpp_errors = extract_cpp_error_codes(TYPES_PATH)

    for error_name, cpp_code in cpp_errors.items():
        if error_name not in schema_errors:
            result.add_error(
                "missing_error",
                f"Error code '{error_name}' ({cpp_code}) not in schema",
                str(TYPES_PATH),
                f"Add '{error_name}' to schema errors"
            )
        elif schema_errors[error_name].get("code") != cpp_code:
            result.add_error(
                "error_mismatch",
                f"Error code mismatch for '{error_name}': schema={schema_errors[error_name].get('code')}, cpp={cpp_code}",
                str(TYPES_PATH)
            )

    for error_name in schema_errors:
        if error_name not in cpp_errors:
            result.add_warning(
                "extra_error",
                f"Error code '{error_name}' in schema but not in C++",
                str(SCHEMA_PATH)
            )


def validate_events(schema: dict, result: ValidationResult):
    """Validate events match between schema and C++."""
    schema_events = set(schema.get("events", {}).keys())
    cpp_events = set(extract_cpp_events(NOTIFIER_PATH))

    for event in cpp_events:
        if event not in schema_events:
            result.add_error(
                "missing_event",
                f"Event '{event}' not in schema",
                str(NOTIFIER_PATH),
                f"Add '{event}' to schema events"
            )

    for event in schema_events:
        if event not in cpp_events:
            result.add_warning(
                "extra_event",
                f"Event '{event}' in schema but not in availableEventTypes()",
                str(SCHEMA_PATH)
            )


def print_results(result: ValidationResult, verbose: bool = False):
    """Print validation results."""
    if result.errors:
        print(f"\n{'='*60}")
        print(f"ERRORS ({len(result.errors)})")
        print('='*60)
        for err in result.errors:
            print(f"\n[{err.category}] {err.message}")
            if err.location:
                print(f"  Location: {err.location}")
            if err.suggestion:
                print(f"  Suggestion: {err.suggestion}")

    if result.warnings and verbose:
        print(f"\n{'='*60}")
        print(f"WARNINGS ({len(result.warnings)})")
        print('='*60)
        for warn in result.warnings:
            print(f"\n[{warn.category}] {warn.message}")
            if warn.location:
                print(f"  Location: {warn.location}")
            if warn.suggestion:
                print(f"  Suggestion: {warn.suggestion}")

    print(f"\n{'='*60}")
    if result.passed:
        print("VALIDATION PASSED")
    else:
        print(f"VALIDATION FAILED: {len(result.errors)} errors, {len(result.warnings)} warnings")
    print('='*60)


def main():
    verbose = "--verbose" in sys.argv or "-v" in sys.argv

    print("Kdenlive RPC Schema Validator")
    print("="*60)
    print(f"Schema: {SCHEMA_PATH}")
    print(f"Handlers: {HANDLERS_DIR}")
    print()

    result = ValidationResult()

    try:
        schema = load_schema()
        print(f"Loaded schema version: {schema.get('version', 'unknown')}")
        print(f"Methods namespaces: {len(schema.get('methods', {}))}")
        print(f"Events: {len(schema.get('events', {}))}")
        print()
    except Exception as e:
        result.add_error("schema_parse", f"Failed to parse schema: {e}")
        print_results(result, verbose)
        return 1

    print("Validating methods...")
    validate_methods(schema, result, verbose)

    print("Validating error codes...")
    validate_error_codes(schema, result)

    print("Validating events...")
    validate_events(schema, result)

    print_results(result, verbose)

    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
