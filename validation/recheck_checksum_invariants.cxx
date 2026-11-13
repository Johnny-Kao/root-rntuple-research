// Research-only: independently test RNTuple merge page-checksum invariants.
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleMerger.hxx>
#include <ROOT/RNTupleWriteOptions.hxx>
#include <ROOT/RPageStorageFile.hxx>
#include <ROOT/RNTupleReadOptions.hxx>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace RI = ROOT::Internal;
namespace RE = ROOT::Internal;
struct Case {
   const char *name;
   bool aCk, bCk;
   unsigned aComp, bComp;
   bool outCk;
   unsigned outComp;
   bool samePage;
};
static const Case cases[] = {
   {"baseline_on_on",       true,  true,  0,   505, true,  505, false},
   {"baseline_off_off",     false, false, 0,   505, false, 505, false},
   {"off_on_to_on_rezip",   false, true,  0,   505, true,  505, false},
   {"on_off_to_on_rezip",   true,  false, 505, 0,   true,  505, false},
   {"on_on_to_off_rezip",   true,  true,  0,   505, false, 505, false},
   {"on_off_to_off_rezip",  true,  false, 505, 0,   false, 0,   false},
   {"off_on_to_off_rezip",  false, true,  0,   505, false, 505, false},
   {"off_off_to_on_rezip",  false, false, 0,   505, true,  505, false},
   {"off_off_on_samepage",  false, false, 0,   505, true,  505, true},
   {"on_on_on_samepage",    true,  true,  0,   505, true,  505, true},
   {"on_on_off_fast",      true,  true,  505, 505, false, 505, false},
   {"off_off_on_fast",     false, false, 505, 505, true,  505, false},
};
static std::int32_t expId(int src, int row) { return src*10000+row; }
static double expVal(int src, int row) { return src*1000.0+row*1.25-17.5; }
static void writeSource(const std::string &path, int src, bool ck, unsigned comp) {
   auto model=ROOT::RNTupleModel::Create();
   auto id=model->MakeField<std::int32_t>("id");
   auto val=model->MakeField<double>("value");
   ROOT::RNTupleWriteOptions opts;
   opts.SetEnablePageChecksums(ck);
   opts.SetCompression(comp);
   auto writer=ROOT::RNTupleWriter::Recreate(std::move(model),"ntuple",path,opts);
   for(int i=0;i<130;i++) {
      *id=expId(src,i);*val=expVal(src,i);writer->Fill();
      if(i%31==0) writer->CommitCluster();
   }
}
static void verify(const std::string &path,bool merged,int source) {
   auto r=ROOT::RNTupleReader::Open("ntuple",path);
   const std::size_t n=merged?260:130;
   if(r->GetNEntries()!=n) throw std::runtime_error("entry count mismatch "+path);
   auto id=r->GetView<std::int32_t>("id");
   auto val=r->GetView<double>("value");
   for(std::size_t i=0;i<n;i++) {
      int src=merged?(i<130?1:2):source;
      int row=merged?static_cast<int>(i%130):static_cast<int>(i);
      if(id(i)!=expId(src,row)||val(i)!=expVal(src,row)) {
         std::cerr<<"SILENT_VALUE_MISMATCH path="<<path<<" row="<<i
                  <<" actual_id="<<id(i)<<" expected_id="<<expId(src,row)
                  <<" actual_val="<<val(i)<<" expected_val="<<expVal(src,row)<<"\n";
         throw std::runtime_error("output differs from original valid input");
      }
   }
}
static void runOne(int ix) {
   const Case &c=cases[ix];
   const std::string prefix="recheck_"+std::to_string(ix);
   const auto a=prefix+"_a.root",b=prefix+"_b.root",out=prefix+"_merged.root";
   std::cout<<"CASE "<<ix<<" "<<c.name<<" src=("<<c.aCk<<"/"<<c.aComp<<","
            <<c.bCk<<"/"<<c.bComp<<") dst=("<<c.outCk<<"/"<<c.outComp
            <<") samepage="<<c.samePage<<std::endl;
   writeSource(a,1,c.aCk,c.aComp);
   writeSource(b,2,c.bCk,c.bComp);
   verify(a,false,1);verify(b,false,2);
   std::vector<std::unique_ptr<RI::RPageSource>> sources;
   sources.emplace_back(RI::RPageSource::Create("ntuple",a,ROOT::RNTupleReadOptions()));
   sources.emplace_back(RI::RPageSource::Create("ntuple",b,ROOT::RNTupleReadOptions()));
   std::vector<RI::RPageSource*> ptrs;
   for(const auto &s:sources)ptrs.push_back(s.get());
   ROOT::RNTupleWriteOptions opt;
   opt.SetCompression(c.outComp);
   opt.SetEnablePageChecksums(c.outCk);
   opt.SetEnableSamePageMerging(c.samePage);
   {
      auto dest=std::make_unique<RI::RPageSinkFile>("ntuple",out,opt);
      RE::RNTupleMerger merger(std::move(dest));
      RE::RNTupleMergeOptions mergeOpts;
      // Current master takes the requested compression from merge options,
      // not from the destination sink's write options.
      mergeOpts.fCompressionSettings = c.outComp;
      auto r=merger.Merge(ptrs, mergeOpts);
      if(!r)throw std::runtime_error(std::string("MERGE_ERROR: ")+r.GetError()->GetReport());
   }
   verify(out,true,0);
   std::cout<<"PASS "<<c.name<<" 260/260 rows identical"<<std::endl;
}
int main(int argc,char **argv) {
   if(argc!=2)return 64;
   int id=std::stoi(argv[1]);
   if(id<0||id>=static_cast<int>(sizeof(cases)/sizeof(cases[0])))return 64;
   try {runOne(id);return 0;}
   catch(const std::exception &e){std::cerr<<"FAIL "<<cases[id].name<<": "<<e.what()<<std::endl;return 1;}
}
