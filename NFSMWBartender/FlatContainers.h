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

	// Helpers --------------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		// Concept to filter for unique_ptr
		template <typename T>
		struct is_unique_ptr : std::false_type {};

		template <typename T, typename D>
		struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type {};

		template <typename T>
		concept IsUniquePtr = is_unique_ptr<T>::value;



		// Vector wrapper for boilerplate
		template <typename T>
		class Wrapper
		{
		public:

			using value_type = T;

			using self_base      = Wrapper<value_type>;
			using container_type = std::vector<value_type>;
			using size_type      = container_type::size_type;

			using iterator               = container_type::iterator;
			using const_iterator         = container_type::const_iterator;
			using reverse_iterator       = container_type::reverse_iterator;
			using const_reverse_iterator = container_type::const_reverse_iterator;



		protected:
			
			container_type data;


			constexpr Wrapper() = default;

			~Wrapper() = default;

		
			constexpr explicit Wrapper(const size_type cap)
			{
				this->reserve(cap);
			}



		public:

			// May invalidate all iterators
			constexpr void reserve(const size_type new_cap)
			{
				this->data.reserve(new_cap);
			}


			// May invalidate all iterators
			constexpr void shrink_to_fit()
			{
				this->data.shrink_to_fit();
			}


			// Invalidates all iterators
			constexpr void clear() noexcept
			{
				this->data.clear();
			}


			constexpr bool empty() const noexcept
			{
				return this->data.empty();
			}


			constexpr size_type size() const noexcept
			{
				return this->data.size();
			}


			constexpr size_type capacity() const noexcept
			{
				return this->data.capacity();
			}


			constexpr iterator begin() {return this->data.begin();}
			constexpr iterator end()   {return this->data.end();}

			constexpr const_iterator begin() const {return this->data.begin();}
			constexpr const_iterator end()   const {return this->data.end();}

			constexpr reverse_iterator rbegin() {return this->data.rbegin();}
			constexpr reverse_iterator rend()   {return this->data.rend();}

			constexpr const_reverse_iterator rbegin() const {return this->data.rbegin();}
			constexpr const_reverse_iterator rend()   const {return this->data.rend();}
		};
	}
	




	// Reasonably STL-compatible flat-Set class -----------------------------------------------------------------------------------------------------

	template <typename T>
	class Set : public Details::Wrapper<T>
	{
	public:

		using base = typename Set::self_base;

		using value_type     = base::value_type;
		using container_type = base::container_type;
		using size_type      = base::size_type;

		using iterator               = base::iterator;
		using const_iterator         = base::const_iterator;
		using reverse_iterator       = base::reverse_iterator;
		using const_reverse_iterator = base::const_reverse_iterator;


		constexpr Set() = default;

		constexpr explicit Set(const size_type cap) : base(cap) {}


		constexpr Set(const std::initializer_list<value_type> init)
		{
			this->reserve(init.size());

			for (const auto& item : init)
				this->insert(item);
		}


		// Invalidates all iterators
		constexpr Set& operator=(const std::initializer_list<value_type> init)
		{
			this->clear();
			this->reserve(init.size());

			for (const auto& item : init)
				this->insert(item);

			return *this;
		}


		template <typename U>
		requires std::equality_comparable_with<U, value_type>
		constexpr iterator find(const U& value)
		{
			return std::find(this->begin(), this->end(), value);
		}


		template <typename U>
		requires std::equality_comparable_with<U, value_type>
		constexpr const_iterator find(const U& value) const
		{
			return std::find(this->begin(), this->end(), value);
		}


		template <typename U>
		requires std::equality_comparable_with<U, value_type>
		constexpr bool contains(const U& value) const
		{
			return (this->find(value) != this->end());
		}


		// May invalidate all iterators
		template <typename ValArg>
		constexpr std::pair<iterator, bool> insert(ValArg&& value)
		{
			const auto it = this->find(value);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::forward<ValArg>(value));

			return {std::prev(this->end()), true};
		}


		// May invalidate all iterators
		template <typename... Args>
		constexpr std::pair<iterator, bool> emplace(Args&&... args)
		{
			value_type temp(std::forward<Args>(args)...);

			const auto it = this->find(temp);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::move(temp));

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename U>
		requires std::equality_comparable_with<U, value_type>
		constexpr bool erase(const U& value)
		{
			const auto it = this->find(value);
			if (it == this->end()) return false;

			this->erase(it);

			return true;
		}


		// Invalidates iterators of erased and final value
		constexpr iterator erase(const iterator it)
		{
			if (it == this->end()) return it;

			const auto last_it = std::prev(this->end());

			if (it != last_it)
				*it = std::move(*last_it);

			this->data.pop_back();

			return it;
		}


		// Invalidates iterators of erased and final value
		constexpr reverse_iterator erase(const reverse_iterator rit)
		{
			if (rit == this->rend()) return rit;
			return {this->erase(std::prev(rit.base()))};
		}
	};

	



	// Similarly STL-compatible flat-Map class ------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class Map : public Details::Wrapper<std::pair<K, V>>
	{
	public:

		using key_type    = K;
		using mapped_type = V;

		using base = typename Map::self_base;

		using value_type     = base::value_type;
		using container_type = base::container_type;
		using size_type      = base::size_type;

		using iterator               = base::iterator;
		using const_iterator         = base::const_iterator;
		using reverse_iterator       = base::reverse_iterator;
		using const_reverse_iterator = base::const_reverse_iterator;


		constexpr Map() = default;

		constexpr explicit Map(const size_type cap) : base(cap) {}


		constexpr Map(const std::initializer_list<value_type> init)
		{
			this->reserve(init.size());

			for (const auto& item : init) 
				this->insert(item.first, item.second);
		}


		// Invalidates all iterators
		constexpr Map& operator=(const std::initializer_list<value_type> init)
		{
			this->clear();
			this->reserve(init.size());

			for (const auto& item : init) 
				this->insert(item.first, item.second);
			
			return *this;
		}

  
		template <typename U>
		requires std::equality_comparable_with<U, key_type>
		constexpr iterator find(const U& key)
		{
			const auto key_matches = [&key](const value_type& p) {return p.first == key;};
			return std::find_if(this->begin(), this->end(), key_matches);
		}


		template <typename U>
		requires std::equality_comparable_with<U, key_type>
		constexpr const_iterator find(const U& key) const
		{
			const auto key_matches = [&key](const value_type& p) {return p.first == key;};
			return std::find_if(this->begin(), this->end(), key_matches);
		}


		template <typename U>
		requires std::equality_comparable_with<U, key_type>
		constexpr bool contains(const U& key) const
		{
			return (this->find(key) != this->end());
		}


		// May invalidate all iterators
		template <typename KeyArg, typename ValArg>
		constexpr std::pair<iterator, bool> insert
		(
			KeyArg&& key, 
			ValArg&& value
		) {
			const auto it = this->find(key);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::forward<KeyArg>(key), std::forward<ValArg>(value));

			return {std::prev(this->end()), true};
		}


		// May invalidate all iterators
		template <typename KeyArg, typename... ValArgs>
		requires (not Details::IsUniquePtr<mapped_type>)
		constexpr std::pair<iterator, bool> try_emplace
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


		// May invalidate all iterators
		template <typename KeyArg, typename... ValArgs>
		requires Details::IsUniquePtr<mapped_type>
		constexpr std::pair<iterator, bool> try_emplace
		(
			KeyArg&&     key, 
			ValArgs&& ...args
		) {
			const auto it = this->find(key);
			if (it != this->end()) return {it, false};

			// Truly lazy construction of type contained in unique pointer
			this->data.emplace_back
			(
				std::forward<KeyArg>(key), 
				std::make_unique<typename mapped_type::element_type>(std::forward<ValArgs>(args)...)
			);

			return {std::prev(this->end()), true};
		}


		// Invalidates iterators of erased and final value
		template <typename U>
		requires std::equality_comparable_with<U, key_type>
		constexpr bool erase(const U& key)
		{
			const auto it = this->find(key);
			if (it == this->end()) return false;

			this->erase(it);

			return true;
		}


		// Invalidates iterators of erased and final value
		constexpr iterator erase(const iterator it)
		{
			if (it == this->end()) return it;

			const auto final_it = std::prev(this->end());

			if (it != final_it)
				*it = std::move(*final_it);

			this->data.pop_back();

			return it;
		}


		// Invalidates iterators of erased and final value
		constexpr reverse_iterator erase(const reverse_iterator rit)
		{
			if (rit == this->rend()) return rit;
			return {this->erase(std::prev(rit.base()))};
		}
	};
}