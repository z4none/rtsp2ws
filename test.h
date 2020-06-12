#pragma once

class Test
{
public:
	bool m_ftyp;
	std::ofstream m_file;

	//
	Test():
		m_ftyp(false)
	{
		m_file = std::ofstream("tttt.mp4", std::ios::binary);
	}

	~Test()
	{
		m_file.close();
	}

	void Save(uint8_t * data, int size)
	{
		m_file.write((const char *)data, size);
	}
};