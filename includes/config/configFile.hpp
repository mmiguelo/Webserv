class configFile {
	private:
		size_t _maxBodySize;
	public:
		configFile();
		configFile(const configFile& other);
		configFile& operator=(const configFile& other);
		~configFile();
		size_t getMaxBodySize();
}