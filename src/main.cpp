import std;
import application;

auto main() -> int
{
	std::println("Current working dir: {}", std::filesystem::current_path().generic_string());

	auto app = gcmt::application();

	return app.run();
}
