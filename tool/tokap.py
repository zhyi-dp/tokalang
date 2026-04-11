#!/usr/bin/env python3
import os
import sys
import argparse
import subprocess
import re
import shutil

CACHE_DIR = os.path.expanduser("~/.toka/pkg")

def init_project(args):
    if os.path.exists("toka.toml"):
        print("Error: toka.toml already exists!")
        sys.exit(1)
    
    with open("toka.toml", "w") as f:
        f.write("[package]\n")
        f.write("name = \"app\"\n")
        f.write("version = \"0.1.0\"\n\n")
        f.write("[dependencies]\n")
    print("Initialize new Toka package.")

def add_package(args):
    url = args.pkg_url
    # Expecting format like: pkg.tokalang.dev/zhyi/ansi
    parts = url.split('/')
    if len(parts) < 3:
        print("Error: Package URL must be in format domain/user/repo")
        sys.exit(1)
        
    alias = args.alias if args.alias else parts[-1]
    
    # Simple parse of the toml to append dependency
    content = ""
    if os.path.exists("toka.toml"):
        with open("toka.toml", "r") as f:
            content = f.read()
    else:
        content = "[dependencies]\n"
        
    # Very naive regex/string replace for adding/updating dep
    dep_line = f'{alias} = "{url}"\n'
    if "[dependencies]" in content:
        content = content.replace("[dependencies]\n", f"[dependencies]\n{dep_line}")
    else:
        content += f"\n[dependencies]\n{dep_line}"
        
    with open("toka.toml", "w") as f:
        f.write(content)
        
    print(f"Added dependency: {alias} -> {url}")
    sync_packages() # download it instantly

def sync_packages():
    """Reads toka.toml and fetches all packages to local cache"""
    if not os.path.exists("toka.toml"):
        return {}
        
    dependencies = {}
    with open("toka.toml", "r") as f:
        in_deps = False
        for line in f:
            line = line.strip()
            if line.startswith("[dependencies]"):
                in_deps = True
                continue
            if line.startswith("["):
                in_deps = False
                continue
            if in_deps and "=" in line:
                alias = line.split("=")[0].strip()
                url = line.split("=")[1].strip().strip('"').strip("'")
                dependencies[alias] = url

    for alias, url in dependencies.items():
        # Mock download: If URL is pkg.tokalang.dev/zhyi/ansi, we clone from local mock or github
        cache_path = os.path.join(CACHE_DIR, url)
        if not os.path.exists(cache_path):
            print(f"Fetching {url}...")
            os.makedirs(os.path.dirname(cache_path), exist_ok=True)
            
            # SIMULATION: Because we don't really have the CF proxy in this dev environment,
            # we will pretend doing a git clone by copying from a known local folder OR cloning from github.
            # If the user provides a real github, we clone it
            github_url = f"https://github.com/{urllib_proxy(url)}"
            
            # Since we are mock testing locally, we'll try to find if there is a local 'examples/libs/ansi'
            mock_local = f"/Users/zhyi/GitDP/tokalang/examples/libs/{url.split('/')[-1]}"
            if os.path.exists(mock_local):
                print(f" (Simulation) Copying from local mock {mock_local} instead of network")
                shutil.copytree(mock_local, cache_path)
            else:
                # Fallback to actual git clone if it exists online
                cmd = ["git", "clone", "--depth", "1", github_url, cache_path]
                # print(f"Executing: {' '.join(cmd)}")
                # res = subprocess.run(cmd, capture_output=True)
                print(f"Error: Mock local repo {mock_local} not found! Create it first or implement real clone.")
                sys.exit(1)
    
    return dependencies

def urllib_proxy(url):
    # Mock CF resolver: pkg.tokalang.dev/zhyi/ansi -> zhyi-dp/ansi
    # (Just making things work for the POC shell)
    parts = url.split("/")
    if len(parts) >= 3:
        return f"{parts[1]}-dp/{parts[2]}"
    return url

def build_project(args):
    print("tokap: Synchronizing dependencies...")
    deps = sync_packages()
    
    # Create the link directory
    link_dir = ".tokapkg"
    if os.path.exists(link_dir):
        shutil.rmtree(link_dir)
    os.makedirs(link_dir, exist_ok=True)
    
    for alias, url in deps.items():
        # The package must have a src/lib.tk or similar
        pkg_src = os.path.join(CACHE_DIR, url, "src", "lib.tk")
        if not os.path.exists(pkg_src):
            pkg_src = os.path.join(CACHE_DIR, url, f"{url.split('/')[-1]}.tk") # alternative layout
        if not os.path.exists(pkg_src):
            print(f"Error: Cannot find entry point for package {alias} at {os.path.join(CACHE_DIR, url)}")
            sys.exit(1)
            
        link_target = os.path.abspath(pkg_src)
        link_name = os.path.join(link_dir, f"{alias}.tk")
        os.symlink(link_target, link_name)
    
    # Execute actual tokac
    build_dir = "/Users/zhyi/GitDP/tokalang/build/src"
    tokac_bin = os.path.join(build_dir, "tokac")
    
    # We pass the entry point. Default to src/main.tk
    entry = "src/main.tk"
    if not os.path.exists(entry):
        entry = args.input if hasattr(args, 'input') and args.input else "main.tk"
        
    cmd = [tokac_bin, "-I", link_dir, entry]
    print(f"tokap: Building via: {' '.join(cmd)}")
    sys.exit(subprocess.call(cmd))

def main():
    parser = argparse.ArgumentParser(description="Toka Package Manager (tokap)")
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    pm_init = subparsers.add_parser("init")
    pm_init.set_defaults(func=init_project)
    
    pm_add = subparsers.add_parser("add")
    pm_add.add_argument("pkg_url", help="Package URL namespace")
    pm_add.add_argument("--alias", help="Alias for the package", default=None)
    pm_add.set_defaults(func=add_package)
    
    pm_build = subparsers.add_parser("build")
    pm_build.set_defaults(func=build_project)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
