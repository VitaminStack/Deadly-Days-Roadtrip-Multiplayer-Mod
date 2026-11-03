"""Read-only-Referenzinventar und begrenzter Bytecode-Audit, Build 26187885.

Kein Codeexport: keine Scriptbytes, Rohzeiger oder rekonstruierten Funktionskoerper.
Token-Kandidaten und validierte Ausdrucks-/Sprunggrenzen werden getrennt gemeldet.
Auch ein vollstaendig gelesener Graph ersetzt keinen Host-/Client-Ereignistest.
"""
import argparse
import json
import struct

DEFAULT_OWNERS = ["BP_EnemySpawner_C", "BP_Enemy_C", "BP_Projectile_C",
                  "BP_PowerUpCrate_C", "BP_Airdrop_Lootable_C", "WP_UI_C",
                  "BP_Effect_Add_Explosive_Damage_C", "BP_Effect_Add_Poison_Cloud_C",
                  "BP_Effect_Add_ImpactEffect_Template_C", "BP_Impact_Explosion_Damage_C",
                  "BP_Impact_Poison_Cloud_C"]
# EExprToken: EX_FinalFunction, EX_LocalFinalFunction, EX_CallMath.
CALL_TOKENS = {0x1C, 0x46, 0x68}


def candidate_calls(script, functions):
    """Bekannte UFunction-Pointer nach Call-Token suchen; keine Token-Grenzvalidierung."""
    result = set()
    for offset in range(1, len(script) - 7):
        if script[offset - 1] in CALL_TOKENS:
            pointer = struct.unpack_from("<Q", script, offset)[0]
            if pointer in functions:
                result.add(functions[pointer])
    return sorted(result)


def main():
    from ddmem import Game  # Windows-Zugriff nur bei tatsaechlicher Prozessanalyse.
    from kismet_audit import audit

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--owners", nargs="+", default=DEFAULT_OWNERS)
    args = parser.parse_args()
    game = Game(args.pid)
    functions, selected, class_cache = {}, [], {}
    owners = {owner: {"loaded": False, "functions": []} for owner in args.owners}
    for obj in game.objects():
        cls = game.ptr(obj + 0x10)
        if cls not in class_cache:
            class_cache[cls] = game.name(cls)
        kind = class_cache[cls]
        if kind and kind.endswith("BlueprintGeneratedClass"):
            owner = game.name(obj)
            if owner in owners:
                owners[owner]["loaded"] = True
                owners[owner]["parent_class"] = game.name(game.ptr(obj + 0x40))
                default = game.ptr(obj + 0x118)
                current, actor = obj, False
                for _ in range(40):
                    if not current:
                        break
                    actor = actor or game.name(current) == "Actor"
                    current = game.ptr(current + 0x40)
                if default and actor:
                    rep = game.read(default + 0x5B, 1)
                    owners[owner]["actor_replicates_default"] = bool(rep[0] & 8) if rep else None
                if default and owner in ("BP_Effect_Add_Explosive_Damage_C",
                                         "BP_Effect_Add_Poison_Cloud_C"):
                    # ImpactActor aus dem gemeinsamen Blueprint-Parent, SDK-Offset.
                    owners[owner]["impact_actor_default"] = game.name(game.ptr(default + 0x40))
        if kind not in ("Function", "DelegateFunction", "SparseDelegateFunction"):
            continue
        owner = game.name(game.ptr(obj + 0x20))
        name = game.name(obj)
        functions[obj] = f"{owner}.{name}"
        if owner in owners:
            selected.append((obj, owner, name))
    scripts = {}
    for obj, owner, name in selected:
        pointer, count = game.ptr(obj + 0x60), game.i32(obj + 0x68)
        if 0 <= count <= 2_000_000:
            script = game.read(pointer, count) if count else b""
            if script is not None and len(script) == count:
                scripts[f"{owner}.{name}"] = script
    for obj, owner, name in selected:
        flags = game.i32(obj + 0xB0)
        pointer, count = game.ptr(obj + 0x60), game.i32(obj + 0x68)
        row = {"name": name, "function_flags": f"0x{flags & 0xFFFFFFFF:08x}",
               "native": bool(flags & 0x400), "net": bool(flags & 0x40),
               "script_size": count, "read_complete": False, "candidate_calls": []}
        if 0 <= count <= 2_000_000:
            script = game.read(pointer, count) if count else b""
            if script is not None and len(script) == count:
                row["read_complete"] = True
                row["candidate_calls"] = candidate_calls(script, functions)
                row["bytecode_audit"] = audit(script, functions, scripts)
        owners[owner]["functions"].append(row)
    print(json.dumps({"world": game.name(game.world()), "owners": owners}, indent=2))


if __name__ == "__main__":
    main()
