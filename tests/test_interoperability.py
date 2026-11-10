"""Check emitted standard formats with parsers independent of olib."""

import json
import os
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

try:
    import tomllib
except ImportError:
    tomllib = None

try:
    import yaml
except ImportError:
    yaml = None


def convert(executable, source, destination):
    result = subprocess.run([executable, str(source), str(destination)],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode:
        raise RuntimeError(f"conversion failed ({result.returncode}): {source} -> {destination}\n"
                           f"stdout: {result.stdout}\nstderr: {result.stderr}")


def main():
    executable = sys.argv[1]
    if os.name == "nt" and len(sys.argv) > 2:
        os.environ["PATH"] = sys.argv[2] + os.pathsep + os.environ.get("PATH", "")
    expected = {"name": "Ada & Co <x>", "count": -42, "active": True,
                "values": [1, 2, 3]}
    with tempfile.TemporaryDirectory(prefix="olib-interop-") as directory:
        root = pathlib.Path(directory)
        source = root / "external.json"
        source.write_text(json.dumps(expected), encoding="utf-8")

        output = root / "output.json"
        convert(executable, source, output)
        assert json.loads(output.read_text(encoding="utf-8")) == expected

        output = root / "output.xml"
        convert(executable, source, output)
        document = ET.parse(output).getroot()
        assert document.tag == "olib"
        fields = {item.attrib["name"]: item for item in document.find("struct")}
        assert set(fields) == set(expected)
        assert fields["name"].text == expected["name"]
        assert int(fields["count"].text) == expected["count"]
        assert fields["active"].text == "true"
        assert [int(item.text) for item in fields["values"]] == expected["values"]

        whitespace_key = "key\t\n\rvalue"
        whitespace_value = "text\t\n\rvalue"
        xml_source = root / "xml-whitespace.json"
        xml_source.write_text(json.dumps({whitespace_key: whitespace_value}), encoding="utf-8")
        convert(executable, xml_source, output)
        field = ET.parse(output).getroot().find("struct/key")
        assert field.attrib["name"] == whitespace_key
        assert field.text == whitespace_value

        output = root / "output.toml"
        convert(executable, source, output)
        if tomllib:
            with output.open("rb") as stream:
                assert tomllib.load(stream) == expected
            external = root / "external.toml"
            external.write_text('name = "Ada"\ncount = -42\nactive = true\nvalues = [1, 2, 3]\n',
                                encoding="utf-8")
            convert(executable, external, root / "from-toml.json")
            assert json.loads((root / "from-toml.json").read_text(encoding="utf-8")) == {
                **expected, "name": "Ada"}
        else:
            print("SKIP: TOML validation requires Python 3.11 or newer")

        output = root / "output.yaml"
        convert(executable, source, output)
        if yaml:
            assert yaml.safe_load(output.read_text(encoding="utf-8")) == expected
            tricky_keys = {"simple": 0, "a space": 1, "x: y": 2, '"quoted"': 3,
                           "": 4, "true": 5, "123": 6, "line\nbreak": 7,
                           "tab\tkey": 8, "hash#tag": 9, "trailing ": 10}
            source.write_text(json.dumps(tricky_keys), encoding="utf-8")
            convert(executable, source, output)
            assert yaml.safe_load(output.read_text(encoding="utf-8")) == tricky_keys
            external = root / "external.yaml"
            external.write_text('"a space": 1\n"x: y": 2\n"\\\"quoted\\\"": 3\n'
                                '"": 4\n"true": 5\n"123": 6\n'
                                '"line\\nbreak": 7\n"tab\\tkey": 8\n', encoding="utf-8")
            convert(executable, external, root / "from-yaml.json")
            assert json.loads((root / "from-yaml.json").read_text(encoding="utf-8")) == {
                key: tricky_keys[key] for key in ("a space", "x: y", '"quoted"', "",
                                                   "true", "123", "line\nbreak", "tab\tkey")}
        else:
            print("SKIP: YAML validation requires PyYAML")


if __name__ == "__main__":
    main()
