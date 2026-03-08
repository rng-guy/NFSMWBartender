#pragma once

#include <tuple>
#include <vector>
#include <memory>
#include <utility>
#include <concepts>
#include <iterator>
#include <algorithm>
#include <type_traits>
#include <initializer_list>



namespace FlatContainers
{

	// Implementation details -----------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		template <typename T>
		struct is_unique_ptr : std::false_type {};

		template <typename T, typename D>
		struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type {};

		template <typename T>
		concept IsUniquePtr = is_unique_ptr<T>::value;
	}
	




	// STL-"inspired" (i.e. shamelessly stolen) flat Set class --------------------------------------------------------------------------------------

	template <typename T>
	class Set
	{
	public:

		using value_type     = T;
		using container_type = std::vector<value_type>;

		using iterator               = container_type::iterator;
		using const_iterator         = container_type::const_iterator;
		using reverse_iterator       = container_type::reverse_iterator;
		using const_reverse_iterator = container_type::const_reverse_iterator;



	private:

		container_type data;



	public:

		Set() = default;


		explicit Set(const size_t cap)
		{
			this->reserve(cap);
		}


		Set(const std::initializer_list<value_type> init)
		{
			this->reserve(init.size());

			for (const auto& item : init)
				this->insert(item);
		}


		// Invalidates all iterators
		Set& operator=(const std::initializer_list<value_type> init)
		{
			this->clear();
			this->reserve(init.size());

			for (const auto& item : init)
				this->insert(item);

			return *this;
		}


		// May invalidate all iterators
		void reserve(const size_t new_cap)
		{
			this->data.reserve(new_cap);
		}


		// May invalidate all iterators
		void shrink_to_fit()
		{
			this->data.shrink_to_fit();
		}


		// Invalidates all iterators
		void clear() noexcept
		{
			this->data.clear();
		}


		bool empty() const noexcept
		{
			return this->data.empty();
		}


		size_t size() const noexcept
		{
			return this->data.size();
		}


		template <typename U>
		requires std::equality_comparable_with<U, T>
		iterator find(const U& value)
		{
			return std::find(this->begin(), this->end(), value);
		}


		template <typename U>
		requires std::equality_comparable_with<U, T>
		const_iterator find(const U& value) const
		{
			return std::find(this->begin(), this->end(), value);
		}


		template <typename U>
		requires std::equality_comparable_with<U, T>
		bool contains(const U& value) const
		{
			return (this->find(value) != this->end());
		}


		// May invalidate all iterators
		template <typename ValArg>
		std::pair<iterator, bool> insert(ValArg&& value)
		{
			const auto it = this->find(value);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::forward<ValArg>(value));

			return {std::prev(this->end()), true};
		}


		// May invalidate all iterators
		template <typename... Args>
		std::pair<iterator, bool> emplace(Args&&... args)
		{
			T temp(std::forward<Args>(args)...);

			const auto it = this->find(temp);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::move(temp));

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename U>
		requires std::equality_comparable_with<U, T>
		bool erase(const U& value)
		{
			const auto it = this->find(value);
			if (it == this->end()) return false;

			this->erase(it);

			return true;
		}


		// Invalidates iterators of erased and final value
		iterator erase(const iterator it)
		{
			if (it == this->end()) return it;

			const auto last_it = std::prev(this->end());

			if (it != last_it)
				*it = std::move(*last_it);

			this->data.pop_back();

			return it;
		}


		// Invalidates iterators of erased and final value
		reverse_iterator erase(const reverse_iterator rit)
		{
			if (rit == this->rend()) return rit;

			return reverse_iterator(this->erase(std::prev(rit.base())));
		}


		iterator begin() {return this->data.begin();}
		iterator end()   {return this->data.end();}

		const_iterator begin() const {return this->data.begin();}
		const_iterator end()   const {return this->data.end();}

		reverse_iterator rbegin() {return this->data.rbegin();}
		reverse_iterator rend()   {return this->data.rend();}

		const_reverse_iterator rbegin() const {return this->data.rbegin();}
		const_reverse_iterator rend()   const {return this->data.rend();}
	};





	// Similarly "inspired" flat Map class ----------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class Map 
	{
	public:

		using key_type       = K;
		using mapped_type    = V;
		using value_type     = std::pair<K, V>;
		using container_type = std::vector<value_type>;

		using iterator               = container_type::iterator;
		using const_iterator         = container_type::const_iterator;
		using reverse_iterator       = container_type::reverse_iterator;
		using const_reverse_iterator = container_type::const_reverse_iterator;



	private:

		container_type data;



	public:

		Map() = default;

		
		explicit Map(const size_t cap)
		{
			this->reserve(cap);
		}


		Map(const std::initializer_list<value_type> init)
		{
			this->reserve(init.size());

			for (const auto& item : init) 
				this->insert(item.first, item.second);
		}


		// Invalidates all iterators
		Map& operator=(const std::initializer_list<value_type> init)
		{
			this->clear();
			this->reserve(init.size());

			for (const auto& item : init) 
				this->insert(item.first, item.second);
			
			return *this;
		}

   
		// May invalidate all iterators
		void reserve(const size_t new_cap)
		{ 
			this->data.reserve(new_cap); 
		}


		// May invalidate all iterators
		void shrink_to_fit()
		{
			this->data.shrink_to_fit();
		}


		// Invalidates all iterators
		void clear() noexcept 
		{ 
			this->data.clear(); 
		}


		bool empty() const noexcept 
		{ 
			return this->data.empty(); 
		}


		size_t size() const noexcept 
		{ 
			return this->data.size(); 
		}

  
		template <typename U>
		requires std::equality_comparable_with<U, K>
		iterator find(const U& key) 
		{
			const auto key_matches = [&key](const value_type& p) {return p.first == key;};

			return std::find_if(this->begin(), this->end(), key_matches);
		}


		template <typename U>
		requires std::equality_comparable_with<U, K>
		const_iterator find(const U& key) const 
		{
			const auto key_matches = [&key](const value_type& p) {return p.first == key;};

			return std::find_if(this->begin(), this->end(), key_matches);
		}


		template <typename U>
		requires std::equality_comparable_with<U, K>
		bool contains(const U& key) const 
		{
			return (this->find(key) != this->end());
		}


		// Invalidates iterators of erased and final value
		template <typename KeyArg, typename ValArg>
		std::pair<iterator, bool> insert
		(
			KeyArg&& key, 
			ValArg&& value
		) {
			const auto it = this->find(key);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::forward<KeyArg>(key), std::forward<ValArg>(value));

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename KeyArg, typename... ValArgs>
		requires (not Details::IsUniquePtr<V>)
		std::pair<iterator, bool> try_emplace
		(
			KeyArg&&     key, 
			ValArgs&& ...args
		) {
			const auto it = this->find(key);
			if (it != this->end()) return {it, false};

			this->data.emplace_back
			(
				std::piecewise_construct, 
				std::forward_as_tuple(std::forward<KeyArg> (key)), 
				std::forward_as_tuple(std::forward<ValArgs>(args)...)
			);

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename KeyArg, typename... ValArgs>
		requires Details::IsUniquePtr<V>
		std::pair<iterator, bool> try_emplace
		(
			KeyArg&&     key, 
			ValArgs&& ...args
		) {
			const auto it = this->find(key);
			if (it != this->end()) return {it, false};

			// Truly lazy construction of type contained in unique pointer
			this->data.emplace_back(std::forward<KeyArg>(key), std::make_unique<typename V::element_type>(std::forward<ValArgs>(args)...));

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename U>
		requires std::equality_comparable_with<U, K>
		bool erase(const U& key) 
		{
			const auto it = this->find(key);
			if (it == this->end()) return false;

			this->erase(it);

			return true;
		}


		// Invalidates iterators of erased and final value
		iterator erase(const iterator it)
		{
			if (it == this->end()) return it;

			const auto final_it = std::prev(this->end());

			if (it != final_it)
				*it = std::move(*final_it);

			this->data.pop_back();

			return it;
		}


		// Invalidates iterators of erased and final value
		reverse_iterator erase(const reverse_iterator rit)
		{
			if (rit == this->rend()) return rit;

			return reverse_iterator(this->erase(std::prev(rit.base())));
		}


		iterator begin() {return this->data.begin();}
		iterator end()   {return this->data.end();}

		const_iterator begin() const {return this->data.begin();}
		const_iterator end()   const {return this->data.end();}

		reverse_iterator rbegin() {return this->data.rbegin();}
		reverse_iterator rend()   {return this->data.rend();}

		const_reverse_iterator rbegin() const {return this->data.rbegin();}
		const_reverse_iterator rend()   const {return this->data.rend();}
	};
}