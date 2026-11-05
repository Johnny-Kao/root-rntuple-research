// Diagnostics only: inspect on-disk RNTuple page metadata without loading values.
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleDescriptor.hxx>
#include <iostream>
#include <string>
int main(int argc, char **argv) {
   if (argc != 2) return 64;
   try {
      auto reader = ROOT::RNTupleReader::Open("ntuple", argv[1]);
      const auto &desc = reader->GetDescriptor();
      std::cout << "H1_DESCRIPTOR file=" << argv[1] << std::endl;
      std::size_t count = 0;
      for (const auto &cluster : desc.GetClusterIterable()) {
         for (const auto &col : cluster.GetColumnRangeIterable()) {
            auto colId = col.GetPhysicalColumnId();
            for (const auto &page : cluster.GetPageRange(colId).GetPageInfos()) {
               if (count++ >= 80) return 0;
               std::cout << "H1_PAGE cluster=" << cluster.GetId()
                         << " col=" << colId
                         << " zip=" << col.GetCompressionSettings().value_or(0)
                         << " dataBytes=" << page.GetLocator().GetNBytesOnStorage()
                         << " checksum=" << page.HasChecksum()
                         << " elements=" << page.GetNElements()
                         << std::endl;
            }
         }
      }
      return 0;
   } catch (const std::exception &ex) {
      std::cerr << "H1_DESCRIPTOR_ERROR: " << ex.what() << std::endl;
      return 1;
   }
}
