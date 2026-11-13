// Research-only: end-to-end public TFileMerger path for mixed checksum RNTuples.
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>
#include <TFileMerger.h>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

static void writeSource(const std::string &file, int src, bool checksums, int compression) {
   auto model = ROOT::RNTupleModel::Create();
   auto value = model->MakeField<std::int32_t>("value");
   ROOT::RNTupleWriteOptions options;
   options.SetEnablePageChecksums(checksums);
   options.SetCompression(compression);
   auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "ntuple", file, options);
   for (int i = 0; i < 130; ++i) {
      *value = src * 1000 + i;
      writer->Fill();
      if (i % 31 == 0) writer->CommitCluster();
   }
}
static void verify(const std::string &file, int entries, bool reverseOrder) {
   auto reader = ROOT::RNTupleReader::Open("ntuple", file);
   if (reader->GetNEntries() != static_cast<std::uint64_t>(entries))
      throw std::runtime_error("unexpected number of entries");
   auto view = reader->GetView<std::int32_t>("value");
   for (int i = 0; i < entries; ++i) {
      int src = (i < 130) == reverseOrder ? 2 : 1;
      auto expected = src * 1000 + i % 130;
      if (view(i) != expected) throw std::runtime_error("merged value mismatch at " + std::to_string(i));
   }
}
int main(int argc, char **argv) {
   if (argc != 2) return 64;
   const std::string prefix(argv[1]);
   try {
      writeSource(prefix + "-a.root", 1, false, 0);
      writeSource(prefix + "-b.root", 2, true, 505);
      for (bool reverseOrder : {false, true}) {
         const auto output = prefix + (reverseOrder ? "-reverse.root" : "-forward.root");
         TFileMerger merger;
         if (!merger.OutputFile(output.c_str(), true))
            throw std::runtime_error("OutputFile failed");
         const auto first = prefix + (reverseOrder ? "-b.root" : "-a.root");
         const auto second = prefix + (reverseOrder ? "-a.root" : "-b.root");
         if (!merger.AddFile(first.c_str()) || !merger.AddFile(second.c_str()))
            throw std::runtime_error("AddFile failed");
         if (!merger.Merge()) throw std::runtime_error("TFileMerger::Merge failed");
         verify(output, 260, reverseOrder);
      }
      std::cout << "PUBLIC_TFILEMERGER_PASS 2 orders x 260 rows\n";
      return 0;
   } catch (const std::exception &err) {
      std::cerr << "PUBLIC_TFILEMERGER_FAIL " << err.what() << '\n';
      return 1;
   }
}
