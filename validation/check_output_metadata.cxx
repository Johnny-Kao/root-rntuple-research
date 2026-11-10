// Research-only inspection: persisted page checksum flags, not merely value readback.
#include <ROOT/RPageStorageFile.hxx>
#include <ROOT/RNTupleReadOptions.hxx>
#include <ROOT/RNTupleDescriptor.hxx>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

struct Counts { std::size_t withChecksum = 0, withoutChecksum = 0; };
static Counts inspect(const std::string &path) {
   auto source = ROOT::Internal::RPageSource::Create("ntuple", path, ROOT::RNTupleReadOptions());
   source->Attach();
   source->LoadAllPageLists();
   auto descriptor = source->GetSharedDescriptorGuard();
   Counts counts;
   for (const auto &cluster : descriptor->GetActiveClusterIterable()) {
      std::unordered_set<ROOT::DescriptorId_t> seen;
      for (const auto &column : descriptor->GetColumnIterable()) {
         const auto id = column.GetPhysicalId();
         if (!seen.insert(id).second || !cluster.ContainsColumn(id)) continue;
         for (const auto &page : cluster.GetPageRange(id).GetPageInfos()) {
            if (page.HasChecksum()) ++counts.withChecksum;
            else ++counts.withoutChecksum;
         }
      }
   }
   if (counts.withChecksum + counts.withoutChecksum == 0)
      throw std::runtime_error("No pages inspected: " + path);
   std::cout << "METADATA " << path << " checksum_on=" << counts.withChecksum
             << " checksum_off=" << counts.withoutChecksum << std::endl;
   return counts;
}
int main(int argc, char **argv) {
   if (argc != 4) return 64;
   try {
      auto a = inspect(argv[1]);
      auto b = inspect(argv[2]);
      auto output = inspect(argv[3]);
      if (a.withChecksum || b.withChecksum || !a.withoutChecksum || !b.withoutChecksum)
         throw std::runtime_error("Input control is not wholly checksum-free");
      std::cout << "OUTPUT_WITHOUT_CHECKSUM=" << output.withoutChecksum << std::endl;
      std::cout << "OUTPUT_WITH_CHECKSUM=" << output.withChecksum << std::endl;
      std::cout << "DST_CHECKSUM_OPTION_REQUESTED=true" << std::endl;
      return 0;
   } catch (const std::exception &e) {
      std::cerr << "METADATA_INSPECTION_FAILURE: " << e.what() << std::endl;
      return 1;
   }
}
