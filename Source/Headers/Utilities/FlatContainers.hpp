#pragma once

#include <tuple>
#include <vector>
#include <ranges>
#include <memory>
#include <utility>
#include <concepts>
#include <iterator>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <initializer_list>



namespace FlatContainers
{
	// Helpers --------------------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		// Map-key extractor
		struct FirstOfPair
		{
			template <typename K, typename V>
			[[nodiscard]] constexpr const K& operator()(const std::pair<K, V>& pair) const noexcept
			{
				return pair.first;
			}
		};



		// Vector wrapper for common boilerplate
		template <typename T, class KeyPolicy>
		requires (std::invocable<const KeyPolicy&, const T&> and std::default_initializable<KeyPolicy>)
		class Wrapper
		{
		public: // aliases

			using value_type = T;

			using self_base = Wrapper<value_type, KeyPolicy>;

			using container_type = std::vector<value_type>;
			using size_type      = container_type::size_type;

			using comparison_type = std::invoke_result_t<const KeyPolicy&, const value_type&>;

			using iterator               = container_type::iterator;
			using const_iterator         = container_type::const_iterator;
			using reverse_iterator       = container_type::reverse_iterator;
			using const_reverse_iterator = container_type::const_reverse_iterator;


		protected: // members
			
			container_type data;


		protected: // methods

			constexpr Wrapper() noexcept = default;


		public: // methods

			// May invalidate all iterators
			constexpr void reserve(const size_type capacity)
			{
				this->data.reserve(capacity);
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


			[[nodiscard]] constexpr bool empty() const noexcept
			{
				return this->data.empty();
			}


			[[nodiscard]] constexpr size_type size() const noexcept
			{
				return this->data.size();
			}


			[[nodiscard]] constexpr size_type capacity() const noexcept
			{
				return this->data.capacity();
			}


			[[nodiscard]] constexpr iterator begin() noexcept {return this->data.begin();}
			[[nodiscard]] constexpr iterator end  () noexcept {return this->data.end  ();}

			[[nodiscard]] constexpr const_iterator begin() const noexcept {return this->data.begin();}
			[[nodiscard]] constexpr const_iterator end  () const noexcept {return this->data.end  ();}

			[[nodiscard]] constexpr const_iterator cbegin() const noexcept {return this->data.cbegin();}
			[[nodiscard]] constexpr const_iterator cend  () const noexcept {return this->data.cend  ();}


			[[nodiscard]] constexpr reverse_iterator rbegin() noexcept {return this->data.rbegin();}
			[[nodiscard]] constexpr reverse_iterator rend  () noexcept {return this->data.rend  ();}

			[[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {return this->data.rbegin();}
			[[nodiscard]] constexpr const_reverse_iterator rend  () const noexcept {return this->data.rend  ();}

			[[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {return this->data.crbegin();}
			[[nodiscard]] constexpr const_reverse_iterator crend  () const noexcept {return this->data.crend  ();}


			template <typename KeyArg>
			requires std::equality_comparable_with<KeyArg, comparison_type>
			[[nodiscard]] constexpr const_iterator find(const KeyArg& key) const
			{
				return std::ranges::find(this->data, key, KeyPolicy{});
			}


			template <typename KeyArg>
			requires std::equality_comparable_with<KeyArg, comparison_type>
			[[nodiscard]] constexpr iterator find(const KeyArg& key) 
			{
				return this->begin() + (std::as_const(*this).find(key) - this->cbegin());
			}


			template <typename KeyArg>
			requires std::equality_comparable_with<KeyArg, comparison_type>
			[[nodiscard]] constexpr bool contains(const KeyArg& key) const
			{
				return (this->find(key) != this->cend());
			}


			// Invalidates iterators of erased and last element
			constexpr iterator erase(const const_iterator cit)
			{
				if (cit == this->cend()) return this->end();

				const auto index  = cit - this->cbegin();
				const auto lastIt = std::prev(this->end());

				const auto it = this->begin() + index;

				if (it != lastIt)
					*it = std::move(*lastIt);

				this->data.pop_back();

				return this->begin() + index;
			}


			// Invalidates iterators of erased and last element
			constexpr reverse_iterator erase(const const_reverse_iterator crit)
			{
				if (crit == this->crend()) return this->rend();

				const auto cit    = std::prev(crit.base());
				const auto nextIt = this->erase(cit);

				return reverse_iterator(nextIt);
			}


			// Invalidates iterators of erased and last element
			template <typename KeyArg>
			requires std::equality_comparable_with<KeyArg, comparison_type>
			constexpr bool erase(const KeyArg& key)
			{
				const auto it = this->find(key);
				if (it == this->end()) return false;

				this->erase(it);

				return true;
			}


		protected: // methods

			constexpr explicit Wrapper(const size_type capacity)
			{
				this->reserve(capacity);
			}

			
			// Invalidates all iterators
			constexpr void assign(const std::initializer_list<value_type> list)
			{
				this->clear  ();
				this->reserve(list.size());

				const KeyPolicy get_key{};

				for (const value_type& element : list)
				{
					if (not this->contains(get_key(element)))
						this->data.push_back(element);
				}
			}
		};
	}
	




	// Reasonably STL-compatible flat-Set class -----------------------------------------------------------------------------------------------------

	template <typename T>
	class Set : public Details::Wrapper<T, std::identity>
	{
	public: // aliases

		using base = typename Set::self_base;

		using value_type     = base::value_type;
		using container_type = base::container_type;
		using size_type      = base::size_type;

		using comparison_type = base::comparison_type;

		using iterator               = base::iterator;
		using const_iterator         = base::const_iterator;
		using reverse_iterator       = base::reverse_iterator;
		using const_reverse_iterator = base::const_reverse_iterator;


	public: // methods

		constexpr Set() noexcept = default;

		constexpr explicit Set(const size_type capacity) : base(capacity) {}


		// May invalidate all iterators
		template <typename ValArg>
		requires std::constructible_from<value_type, ValArg>
		constexpr std::pair<iterator, bool> insert(ValArg&& value)
		{
			const auto it = this->find(value);
			if (it != this->end()) return {it, false};

			this->data.emplace_back(std::forward<ValArg>(value));

			return {std::prev(this->end()), true};
		}


		constexpr Set(const std::initializer_list<value_type> list)
		{
			this->assign(list);
		}


		// Invalidates all iterators
		constexpr Set& operator=(const std::initializer_list<value_type> list)
		{
			this->assign(list);

			return *this;
		}


		constexpr void swap(Set& other) noexcept
		{
			this->data.swap(other.data);
		}


		// May invalidate all iterators
		template <typename ...ValArgs>
		requires std::constructible_from<value_type, ValArgs...>
		constexpr std::pair<iterator, bool> emplace(ValArgs&&... args)
		{
			return this->insert(value_type(std::forward<ValArgs>(args)...));
		}
	};

	



	// Similarly STL-compatible flat-Map class ------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class Map : public Details::Wrapper<std::pair<K, V>, Details::FirstOfPair>
	{
	public: // aliases

		using key_type    = K;
		using mapped_type = V;

		using base = typename Map::self_base;

		using value_type     = base::value_type;
		using container_type = base::container_type;
		using size_type      = base::size_type;

		using comparison_type = base::comparison_type;

		using iterator               = base::iterator;
		using const_iterator         = base::const_iterator;
		using reverse_iterator       = base::reverse_iterator;
		using const_reverse_iterator = base::const_reverse_iterator;


	public: // methods

		constexpr Map() noexcept = default;

		constexpr explicit Map(const size_type capacity) : base(capacity) {}


		// May invalidate all iterators
		template <typename KeyArg, typename ...ValArgs>
		requires (std::constructible_from<key_type, KeyArg> and std::constructible_from<mapped_type, ValArgs...>)
		constexpr std::pair<iterator, bool> try_emplace
		(
			KeyArg&&     key, 
			ValArgs&& ...args
		) {
			const auto pairIt = this->find(key);
			if (pairIt != this->end()) return {pairIt, false};

			this->data.emplace_back
			(
				std::piecewise_construct, 
				std::forward_as_tuple(std::forward<KeyArg> (key)), 
				std::forward_as_tuple(std::forward<ValArgs>(args)...)
			);

			return {std::prev(this->end()), true};
		}


		// May invalidate all iterators
		template <typename KeyArg, typename ValArg>
		requires (std::constructible_from<key_type, KeyArg> and std::constructible_from<mapped_type, ValArg>)
		constexpr std::pair<iterator, bool> insert
		(
			KeyArg&& key, 
			ValArg&& value
		) {
			return this->try_emplace(std::forward<KeyArg>(key), std::forward<ValArg>(value));
		}


		constexpr Map(const std::initializer_list<value_type> list)
		{
			this->assign(list);
		}


		// Invalidates all iterators
		constexpr Map& operator=(const std::initializer_list<value_type> list)
		{
			this->assign(list);
			
			return *this;
		}

  
		constexpr void swap(Map& other) noexcept
		{
			this->data.swap(other.data);
		}


		template <typename KeyArg>
		requires std::equality_comparable_with<KeyArg, comparison_type>
		[[nodiscard]] constexpr const mapped_type* get(const KeyArg& key) const
		{
			const auto foundKey = this->find(key);
			return (foundKey != this->cend()) ? &(foundKey->second) : nullptr;
		}


		template <typename KeyArg>
		requires std::equality_comparable_with<KeyArg, comparison_type>
		[[nodiscard]] constexpr mapped_type* get(const KeyArg& key)
		{
			return const_cast<mapped_type*>(std::as_const(*this).get(key));
		}
	};
}