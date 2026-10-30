#include <ROOT/RNTupleReader.hxx>
#include <iostream>
#include <exception>

int main(int argc, char **argv)
{
   if (argc != 2) return 64;
   try {
      auto reader = ROOT::RNTupleReader::Open("ntuple", argv[1]);
      std::cout << "Opened entries=" << reader->GetNEntries() << std::endl;
      return 0;
   } catch (const std::exception &ex) {
      std::cerr << "Reader rejected input: " << ex.what() << std::endl;
      return 2;
   }
}
