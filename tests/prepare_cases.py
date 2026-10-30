"""Bounded, private reader invariant check. Input is generated during CI only."""
from pathlib import Path
import shutil
import struct
import sys
import ROOT

def generate(path: Path) -> None:
    ROOT.gInterpreter.Declare(r"""
    #include <ROOT/RNTupleModel.hxx>
    #include <ROOT/RNTupleWriter.hxx>
    #include <cstdint>
    #include <memory>
    #include <string>
    void make_ntuple_anchor_probe(const std::string &path) {
        auto model = ROOT::RNTupleModel::Create();
        auto fld = model->MakeField<std::int32_t>("x");
        auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "ntuple", path);
        *fld = 42;
        writer->Fill();
        writer->CommitDataset();
    }
    """)
    ROOT.make_ntuple_anchor_probe(str(path))

def mutate(src: Path, dst: Path) -> None:
    f = ROOT.TFile.Open(str(src), "READ")
    if not f or f.IsZombie():
        raise RuntimeError("Unable to inspect baseline file")
    k = f.GetKey("ntuple")
    if not k:
        raise RuntimeError("Missing RNTuple anchor TKey")
    seek, keylen = int(k.GetSeekKey()), int(k.GetKeylen())
    objlen, total = int(k.GetObjlen()), int(k.GetNbytes())
    f.Close()
    with src.open("rb") as inp:
        inp.seek(seek)
        head = inp.read(10)
    on_disk_nbytes = struct.unpack(">i", head[:4])[0]
    on_disk_objlen = struct.unpack(">I", head[6:10])[0]
    if (on_disk_nbytes, on_disk_objlen) != (total, objlen):
        raise AssertionError("TKey offset/endianness mismatch")
    if not (78 <= objlen < 512 and 0 < total - keylen <= objlen):
        raise AssertionError(f"Unexpected baseline anchor dimensions: {objlen} {total} {keylen}")
    shutil.copyfile(src, dst)
    # Bounded difference, not an allocation bomb. The low level reader must
    # never read more into a temporary buffer than it allocated.
    new_stored_len = objlen + 256
    with dst.open("r+b") as out:
        out.seek(seek)
        out.write(struct.pack(">i", keylen + new_stored_len))
        out.seek(0, 2)
        out.write(bytes(4096))
    print(f"Anchor offsets verified: seek={seek}, keylen={keylen}, "
          f"original stored={total-keylen}, uncompressed={objlen}, "
          f"declared stored={new_stored_len}", flush=True)

if __name__ == "__main__":
    folder = Path(sys.argv[1])
    folder.mkdir(parents=True, exist_ok=True)
    base, altered = folder / "baseline.root", folder / "length-mismatch.root"
    generate(base)
    mutate(base, altered)
