#include "Discoverer.h"
#include <regex>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <boost/algorithm/string.hpp>     

using namespace new_words_discover;

void Discoverer::process()
{
	std::cout << "proccessing file...\n";
	if (parse_file() < 0)
	{
		return;
	}
	std::cout << "done.\n";

	std::cout << "calculating firmness...\n";
	remove_words_by_firmness();
	std::cout << "done.\n";

	remove_words_by_freq();
	remove_words_single();

	std::cout << "calculating degree of freedom...\n";
	remove_words_by_degree_of_freedom();
	std::cout << "done.\n";

	print();
}

int Discoverer::parse_file()
{
	std::wifstream corpus(filename_);
	if (!corpus.is_open())
	{
		std::wcerr << L"Failed to open " + filename_ + L"!" << std::endl;
		return -1;
	}
	std::wstring paragraph;
#pragma warning(disable:4129)
	std::wregex re(L"\W+|[a-zA-Z0-9]+|\s+|\n+"); // Warning	C4129: Unrecognized character escape sequence.
#pragma warning(default:4129)
	std::cout << "calculating word frequency...\n";
	while (std::getline(corpus, paragraph))
	{
		std::vector<std::wstring> para_vec;
		boost::algorithm::split(para_vec, paragraph, boost::is_any_of(L"����������������������������������"));
		for (auto& segment : para_vec)
		{
			std::wsregex_token_iterator it(segment.begin(), segment.end(), re, -1);
			std::wsregex_token_iterator end_it;
			while (it != end_it)
			{
				std::wstring sentence = *it++;
				if (sentence.size() > 0)
				{
					for (size_t i = 1; i <= thresholds_.max_word_len; i++)
					{
						parse_sentence(sentence, i);
					}
				}
			}
		}
		paragraph.clear();

	}
	std::cout << "done.\n";
	return 0;
}

void Discoverer::parse_sentence(const std::wstring & sentence, size_t word_len)
{
	size_t i = word_len;
	for (size_t j = 0; j + i < sentence.size(); j++)
	{
		auto word = sentence.substr(j, i);

		wchar_t left_adja = 0;
		wchar_t right_adja = 0;
		if (j > 0)
		{
			left_adja = sentence[j - 1];
			std::get<1>(words_[word])[left_adja]++;
		}
		if (j + i < sentence.size())
		{
			right_adja = sentence[j + i];
			std::get<2>(words_[word])[right_adja]++;
		}
		std::get<frequency_t>(words_[word])++;
		tot_frequency_++;
	}
}

void Discoverer::remove_words_by_firmness()
{
	for (auto& word : words_)
	{
		if (word.first.size() > 1)
		{
			calculate_firmness(word);
		}
	}
	for (auto it = words_.begin(); it != words_.end();)
	{
		if (words_.size() > 1 &&
			std::get<firmness_t>(it->second) < thresholds_.firmness_thr)
		{
			it = words_.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void Discoverer::calculate_firmness(std::pair<const std::wstring, word_t>& word)
{
	size_t p_word = std::get<frequency_t>(word.second);
	double min_firmness = 0xFFFFFFFF;
	for (size_t i = 0; i < word.first.size(); i++)
	{
		const auto& left_part = words_[word.first.substr(0, i + 1)];
		const auto& right_part = words_[word.first.substr(i + 1, word.first.size() - i - 1)];
		auto ans = static_cast<double>(p_word) * tot_frequency_ / (std::get<frequency_t>(left_part) * std::get<frequency_t>(right_part));
		if (ans < min_firmness)
		{
			min_firmness = ans;
		}
	}
	std::get<firmness_t>(word.second) = min_firmness;
}

double Discoverer::calculate_degree_of_freedom(const std::pair<std::wstring, word_t>& word)
{
	const auto& left_adjas = std::get<1>(word.second);
	const auto& right_adjas = std::get<2>(word.second);
	return std::min(entropy(left_adjas), entropy(right_adjas));
}

double Discoverer::entropy(const std::unordered_map<wchar_t, frequency_t>& adjacents)
{
	int tot_freq = 0;
	double ret = 0;
	std::for_each(adjacents.begin(), adjacents.end(), [&tot_freq](auto val)
	{
		tot_freq += val.second;
	});
	std::for_each(adjacents.begin(), adjacents.end(), [&tot_freq, &ret](auto& val)
	{
		ret += (-std::log2(static_cast<double>(val.second) / tot_freq) * (static_cast<double>(val.second) / tot_freq));
	});
	return ret;
}

void Discoverer::remove_words_by_freq()
{
	for (auto it = words_.begin(); it != words_.end();)
	{
		if (std::get<frequency_t>(it->second) < thresholds_.freq_thr)
		{
			it = words_.erase(it);
		}
		else
		{
			it++;
		}
	}
}


void Discoverer::remove_words_single()
{
	for (auto it = words_.begin(); it != words_.end();)
	{
		if (it->first.size() < 2)
		{
			it = words_.erase(it);
		}
		else
		{
			it++;
		}
	}

}

void Discoverer::remove_words_by_degree_of_freedom()
{
	for (auto it = words_.begin(); it != words_.end();)
	{
		auto freedom = calculate_degree_of_freedom(*it);
		if (freedom < thresholds_.free_thr)
		{
			it = words_.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void Discoverer::print()
{
	std::vector<std::pair<std::wstring, word_t>> sorted(words_.begin(), words_.end());

	std::sort(sorted.begin(), sorted.end(), [](auto& x, auto& y)
	{
		return std::get<frequency_t>(x.second) < std::get<frequency_t>(y.second);
	});

	auto dot = filename_.find_last_of(L'.');
	std::wstring out_file;	
	out_file = (dot != std::wstring::npos ? filename_.substr(0, dot) : filename_);	
	out_file += L"_out.txt";
	std::wofstream output(out_file);
	if (!output.is_open())
	{
		std::wcerr << L"Failed to open " + out_file + L"!" << std::endl;
		return;
	}
	output << L"total: " << sorted.size() << std::endl;
	for (const auto& s : sorted)
	{
		output << s.first << L" " << std::get<frequency_t>(s.second) << std::endl;
	}
}

