#include "../src/core/camera_batch_policy.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
int main() {
    using camera_batch_policy::complete;
    assert(complete(true,true,64,64));
    assert(!complete(true,true,63,64)); // lectura parcial: fallback directo
    assert(!complete(false,true,64,64));
    assert(!complete(true,false,64,64));
    assert(!complete(true,true,0,64));
    assert(!complete(true,true,0,0));
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"src";
    std::ifstream camera_file(root/"game/classes/camera.hpp"); assert(camera_file);
    const std::string camera{std::istreambuf_iterator<char>(camera_file),{}};
    assert(camera.find("&batch.matrix_bytes")!=std::string::npos);
    assert(camera.find("&batch.position_bytes")!=std::string::npos);
    assert(camera.find("ExecuteReadScatter(guard.handle,0,false)")!=std::string::npos);
    assert(camera.find("batch.data.position=getPosition()")!=std::string::npos);
    assert(camera.find("ReadOptional(matrix_address,&batch.data.matrix")!=std::string::npos);
    std::ifstream frame_file(root/"features/misc/misc.hpp"); assert(frame_file);
    const std::string frame{std::istreambuf_iterator<char>(frame_file),{}};
    const auto start=frame.find("inline auto UpdateFrameSnapshot(");
    const auto end=frame.find("// has_renderable_unit_metadata",start);
    assert(start!=std::string::npos && end!=std::string::npos);
    const auto snapshot=frame.substr(start,end-start);
    assert(snapshot.find("camera->prepareFrame(hScatter,camera_batch)")!=std::string::npos);
    assert(snapshot.find("camera->finishFrame(camera_batch,scatter_executed)")!=std::string::npos);
    assert(snapshot.find("&local_position,sizeof(local_position),&local_position_bytes")!=std::string::npos);
    assert(snapshot.find("local_position_bytes==sizeof(local_position)")!=std::string::npos);
    assert(snapshot.find("local_position : local.getPosition()")!=std::string::npos);
    assert(snapshot.find("scatter_executed=TargetProcess->ExecuteReadScatter(hScatter,0,false)")!=std::string::npos);
    assert(snapshot.find("camera->getFrame()") ==std::string::npos);
    assert(snapshot.find("units.empty()?nullptr") ==std::string::npos);
    assert(snapshot.find("if (hScatter)") !=std::string::npos);
    assert(snapshot.find("CreateScatterHandle()") ==snapshot.rfind("CreateScatterHandle()"));
    std::puts("PASS camera matrix/position batching and partial-read fallbacks");
}
