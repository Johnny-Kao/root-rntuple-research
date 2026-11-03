#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>
#include <TFileMerger.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Row { std::int32_t id; double value; };
static std::vector<Row> expected(int source) {
   std::vector<Row> result;
   for (int i = 0; i < 130; ++i)
      result.push_back({source * 10000 + i, source * 1000.0 + i * 1.25 - 17.5});
   return result;
}
static void writeSource(const std::string &path, int source, bool checksums, int compression) {
   auto model = ROOT::RNTupleModel::Create();
   auto id = model->MakeField<std::int32_t>("id");
   auto value = model->MakeField<double>("value");
   ROOT::RNTupleWriteOptions opt;
   opt.SetEnablePageChecksums(checksums);
   opt.SetCompression(compression);
   auto writer = ROOT::RNTupleWriter::Recreate(std::move(model), "ntuple", path, opt);
   for (auto row : expected(source)) {
      *id = row.id;
      *value = row.value;
      writer->Fill();
      if (row.id % 31 == 0) writer->CommitCluster();
   }
}
static void verify(const std::string &path, const std::vector<Row> &rows) {
   auto reader = ROOT::RNTupleReader::Open("ntuple", path);
   if (reader->GetNEntries() != rows.size())
      throw std::runtime_error("entry count mismatch for " + path);
   auto id = reader->GetView<std::int32_t>("id");
   auto value = reader->GetView<double>("value");
   for (std::size_t i = 0; i < rows.size(); ++i) {
      if (id(i) != rows[i].id || value(i) != rows[i].value)
         throw std::runtime_error("SILENT VALUE MISMATCH at " + path + " row " + std::to_string(i));
   }
}
static void run(const std::string &prefix, bool aChecksum, bool bChecksum, int aCompression, int bCompression) {
   const auto a = prefix + "_a.root", b = prefix + "_b.root", out = prefix + "_merged.root";
   writeSource(a, 1, aChecksum, aCompression);
   writeSource(b, 2, bChecksum, bCompression);
   verify(a, expected(1));
   verify(b, expected(2));
   {
      TFileMerger merger;
      if (!merger.AddFile(a.c_str()) || !merger.AddFile(b.c_str()))
         throw std::runtime_error("unable to add input file");
      if (!merger.OutputFile(out.c_str()))
         throw std::runtime_error("unable to create output file");
      if (!merger.Merge())
         throw std::runtime_error("merge failed (not silent corruption)");
   }
   auto rows = expected(1);
   auto tail = expected(2);
   rows.insert(rows.end(), tail.begin(), tail.end());
   verify(out, rows);
   std::cout << "PASS " << prefix << " 260 rows exact" << std::endl;
}
int main() {
   try {
      run("checksum_off_to_on", false, true, 0, 505);
      run("checksum_on_to_off", true, false, 505, 0);
      run("both_checksums_on", true, true, 0, 505);
      run("both_checksums_off", false, false, 505, 0);
      return 0;
   } catch (const std::exception &e) {
      std::cerr << "FAIL " << e.what() << std::endl;
      return 1;
   }
}
