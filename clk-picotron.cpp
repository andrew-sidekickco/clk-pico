//
//  clk-picotron.cpp
//
//  Created by Andrew Docking on 26/02/2026.
//

#include "Analyser/Static/Acorn/Target.hpp"
#include "Machines/Utility/MachineForTarget.hpp"
#include "Outputs/ScanTargets/BufferingScanTarget.hpp"

#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include <filesystem>

struct Binary
{
	std::unique_ptr <std::uint8_t[]> _data;
	std::size_t _size = 0;
};

struct Image
{
	std::pair <std::int32_t, std::int32_t> _size = { 0, 0 };
	const std::uint32_t *_data;
	bool isValid() const { return _data != nullptr && _size.first > 0 && _size.second > 0; }
};

bool saveAs( const Binary& content, const std::string& uri )
{
	if ( content._data == nullptr || content._size == 0 )
	{
		return false;
	}
	
	// Use C++ std::ofstream to write to file.
	
	std::ofstream outFile( uri, std::ios::out | std::ios::binary );
	
	if ( !outFile.is_open() )
	{
		return false;
	}
	
	// Write the binary content to the file.
	
	outFile.write( reinterpret_cast<const char*>(content._data.get()), content._size );
	
	auto error = outFile.bad();
	outFile.close();
	
	return !error;
}

size_t distance(const size_t begin, const size_t end, const size_t buffer_length)
{
	return end >= begin ? end - begin : buffer_length + end - begin;
}

Binary asTGA( const Image& image )
{
	// Check if the image is valid.
	
	if ( !image.isValid() )
	{
		return {};
	}
	
	auto width = image._size.first;
	auto height = image._size.second;
	
	// Calculate total size: 18 bytes for header + 4 bytes per pixel.
	
	auto size = 18 + ( width * height * sizeof( std::uint32_t ) );
	auto result = Binary{ std::make_unique <std::uint8_t[]> ( size ), size };
	
	// Create TGA header (18 bytes).
	
	auto* header = result._data.get();
	std::fill_n(header, 18, 0);     // Initialize all header bytes to zero
	
	// Set important header values
	header[2] = 2;                      // Uncompressed RGB/RGBA
	header[12] = width & 0xFF;          // Width (low byte)
	header[13] = (width >> 8) & 0xFF;   // Width (high byte)
	header[14] = height & 0xFF;         // Height (low byte)
	header[15] = (height >> 8) & 0xFF;  // Height (high byte)
	header[16] = 32;                    // Bits per pixel (32 for RGBA)
	header[17] = 0x28;                  // Image descriptor (0x20 for top-left origin, 0x08 for 8-bit alpha)
	
	// Access image data directly as bytes.
	
	auto *bytes = reinterpret_cast <const std::uint8_t*> ( image._data );
	
	// TGA stores pixels in BGRA order (instead of RGBA). Write pixel data after the header.
	
	auto *pixelData = result._data.get() + 18;
	
	for ( auto y = 0; y < height; y++ )
	{
		for ( auto x = 0; x < width; x++ )
		{
			// Calculate the offset into our RGBA data.
			
			auto sO = ( x + y * width ) << 2;
			auto dO = ( x + y * width ) << 2;
			
			// Write in BGRA order.
			pixelData[dO + 0] = bytes[sO + 2];  // B
			pixelData[dO + 1] = bytes[sO + 1];  // G
			pixelData[dO + 2] = bytes[sO];      // R
			pixelData[dO + 3] = 0xff;//bytes[sO + 3];  // A
		}
	}
	
	return result;
}


template <typename SourceT>
std::vector <Outputs::Display::BufferingScanTarget::Scan> submit(const size_t begin, const size_t end, const SourceT &source) {
	if(begin == end) {
		return {};
	}

	std::vector <Outputs::Display::BufferingScanTarget::Scan> scans;
	
	size_t buffer_destination = 0;
	const auto submit = [&](const size_t begin, const size_t end) {
//		test_gl([&]{
//			glBufferSubData(
//				GL_ARRAY_BUFFER,
//				buffer_destination,
//				(end - begin) * sizeof(source[0]),
//				&source[begin]
//			);
//		});
		
		for ( auto i = begin; i < end; i ++ )
		{
			scans.push_back( source[i] );
		}
		
		buffer_destination += (end - begin) * sizeof(source[0]);
	};
	if(begin < end) {
		submit(begin, end);
//		return end - begin;
	} else {
		submit(begin, source.size());
		submit(0, end);
//		return source.size() - begin + end;
	}
	
	return scans;
}

std::vector<uint8_t> load(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	
	if ( !file )
	{
		return {};
	}

	const auto size = std::filesystem::file_size(path);
	std::vector<uint8_t> buffer(size);

	file.read(reinterpret_cast<char*>(buffer.data()), size);

	return buffer;
}

int main(int argc, const char * argv[]) {

	Analyser::Static::Acorn::ElectronTarget target;
	Outputs::Display::BufferingScanTarget scanTarget;
	
	constexpr int ScansCount = 500;
	constexpr int LinesCount = 4 * ScansCount;
	constexpr int AreaSize = Outputs::Display::BufferingScanTarget::WriteAreaWidth *
							 Outputs::Display::BufferingScanTarget::WriteAreaHeight * 4;
	
	auto scans = std::make_unique <Outputs::Display::BufferingScanTarget::Scan[]> ( ScansCount );
//	auto lines = std::make_unique <Outputs::Display::BufferingScanTarget::Line[]> ( LinesCount );
	auto area = std::make_unique <std::uint8_t[]> ( AreaSize );
	
	scanTarget.set_scan_buffer( scans.get(), ScansCount );
//	scanTarget.set_line_buffer( lines.get(), LinesCount );
	scanTarget.set_write_area( area.get() );
	
	ROMMachine::ROMFetcher romFetcher = [] ( const ROM::Request &roms) -> ROM::Map
	{
		ROM::Map results;
		
		for ( const auto& description: roms.all_descriptions() )
		{
			for ( const auto& file_name: description.file_names )
			{
				auto data = load( "/Users/adocking/Documents/dev/rgco/projects/clk-picotron/ROMImages/Acorn/" + file_name );
				
				if ( !data.empty() )
				{
					results[ description.name ] = data;
				}
			}
		}
			
		return results;
	};

	Machine::Error error;
	
	auto targets = Analyser::Static::GetTargets( "/Users/adocking/Documents/dev/rgco/projects/games/ChuckieEgg_E.uef" );
//	auto machine = Machine::MachineForTarget( &target, romFetcher, error );
	auto machine = Machine::MachineForTargets( targets, romFetcher, error );
	
	Configurable::Device *configurable_device = machine->configurable_device();
	auto options = configurable_device->get_options();
	configurable_device->set_options( options );
	
//	options->Reflection::set(*options, Configurable::Options::DisplayOptionName, int(Configurable::Display::RGB));
	machine->scan_producer()->set_scan_target( &scanTarget );
	
	bool stop = false;
	auto i = 0;
	
	static constexpr int LineBufferHeight = 2048;
	static constexpr int WriteAreaWidth = 2048;
	static constexpr int WriteAreaHeight = 2048;
	
	std::vector<uint8_t> write_area_texture_;
	std::vector<uint8_t> copiedScans;
	std::array<Outputs::Display::BufferingScanTarget::Scan, LineBufferHeight*5> scan_buffer_{};
	std::array<Outputs::Display::BufferingScanTarget::Line, LineBufferHeight> line_buffer_{};
	
	scanTarget.set_scan_buffer(scan_buffer_.data(), scan_buffer_.size());
	scanTarget.set_line_buffer(line_buffer_.data(), line_buffer_.size());
	
	auto frameWidth = 320;
	auto frameHeight = 512;
	auto frame = std::make_unique <std::uint8_t[]> ( frameWidth * frameHeight );
	auto scanOffset = 0;
	auto frameField = 0;
	
	const auto output_items =
		[&](const size_t begin, const size_t end)
		{
			const auto new_scans = ::submit(begin, end, scan_buffer_);
			
			for ( const auto& scan : new_scans )
			{
				auto x = scan.scan.end_points[0].data_offset;
				auto length = scan.scan.end_points[1].data_offset - scan.scan.end_points[0].data_offset;
				const auto *from = write_area_texture_.data() + x + ( scan.data_y * WriteAreaWidth );
				std::copy( from, from + length, frame.get() + ( scanOffset * frameWidth ) );
				
				scanOffset ++;
			}
		};
	const auto end_field =
		[&](
			const bool was_complete,
			const int field_index,
			const bool is_interlaced
		) {
			if ( was_complete )
			{
				scanOffset = 0;
				frameField = field_index;
			}
		};

#define USING_TEST 	 0
#define USING_EXPORT 1
	
	while ( !stop )
	{
		machine->timed_machine()->set_speed_multiplier( 1.0 );
		machine->timed_machine()->run_for( 1.f / 120.f );
		machine->timed_machine()->flush_output( MachineTypes::TimedMachine::Output::All );
		
		const auto area = scanTarget.get_output_area();
		const auto new_modals = scanTarget.new_modals();
		
		if( bool(new_modals))
		{
			const auto modals = scanTarget.modals();
			const auto data_type_size = Outputs::Display::size_for_data_type( modals.input_data_type );
			const size_t required_size = WriteAreaWidth * WriteAreaHeight * data_type_size;
			
			if (required_size != write_area_texture_.size() )
			{
				write_area_texture_.resize( required_size );
				scanTarget.set_write_area( write_area_texture_.data() );
			}
			
			// reset the frame buffer size;
			
			frameHeight = modals.expected_vertical_lines;
			frame = std::make_unique <std::uint8_t[]> ( frameWidth * frameHeight );
		}
		
		// this calls the output_items and end_field lambda see we can copy out data when it's ready.
		
		scanTarget.output_scans( area, output_items, end_field );
		scanTarget.complete_output_area( area );
		
#if USING_EXPORT
		
		auto w = frameWidth;
		auto h = frameHeight;
		auto data = std::make_unique <std::uint32_t[]> ( w * h );
		
		for ( auto i = 0; i < w * h; i ++ )
		{
			auto c = frame[i];
			auto r = c & 1;
			auto g = c & 2;
			auto b = c & 4;
			
			auto color = ( ( r ) ? 0xff0000 : 0 ) +
						 ( ( g ) ? 0xff00 : 0 ) +
						 ( ( b ) ? 0xff : 0 );
			
			data[i] = color + 0xff000000;
		}
		
		auto image = Image{ { w, h }, data.get() };
		auto binary = asTGA( image );
		saveAs( binary, "/Users/adocking/Desktop/test/frame-" + std::to_string( i & 3 ) + ".tga" );
		
#endif // USING_EXPORT
		
		i ++;
	};
	
    return 0;
}
