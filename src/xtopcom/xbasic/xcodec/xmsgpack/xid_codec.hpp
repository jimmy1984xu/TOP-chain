// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbasic/xid.hpp"

#include <msgpack.hpp>

#include <type_traits>

NS_BEG1(msgpack)
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
NS_BEG1(adaptor)

template <typename TagT, typename IdT>
struct as<top::xnullable_id_t<TagT, IdT>, typename std::enable_if<msgpack::has_as<IdT>::value>::type>
{
    top::xnullable_id_t<TagT, IdT>
    operator()(msgpack::object const& o) const {
        if (o.is_nil()) {
            return top::xnullable_id_t<TagT, IdT>{};
        }

        return o.as<IdT>();
    }
};

template <typename TagT, typename IdT>
struct convert<top::xnullable_id_t<TagT, IdT>> final
{
    msgpack::object const &
    operator()(msgpack::object const & o, top::xnullable_id_t<TagT, IdT> & v) const {
        if (o.is_nil()) {
            v = top::xnullable_id_t<TagT, IdT>{};
        } else {
            IdT t;
            msgpack::adaptor::convert<IdT>()(o, t);
            v = top::xnullable_id_t<TagT, IdT>{ t };
        }

        return o;
    }
};

template <typename TagT, typename IdT>
struct pack<top::xnullable_id_t<TagT, IdT>>
{
    template <typename Stream>
    msgpack::packer<Stream> &
    operator()(msgpack::packer<Stream> & o, top::xnullable_id_t<TagT, IdT> const & message) const {
        if (message.has_value()) {
            o.pack(message.value());
        } else {
            o.pack_nil();
        }

        return o;
    }
};

template <typename TagT, typename IdT>
struct object<top::xnullable_id_t<TagT, IdT>>
{
    void
    operator()(msgpack::object & o, top::xnullable_id_t<TagT, IdT> const & message) const {
        if (message.has_value()) {
            msgpack::adaptor::object<IdT>()(o, message.value());
        } else {
            o.type = msgpack::type::NIL;
        }
    }
};

template <typename TagT, typename IdT>
struct object_with_zone<top::xnullable_id_t<TagT, IdT>>
{
    void
    operator()(msgpack::object::with_zone & o, top::xnullable_id_t<TagT, IdT> const & message) const {
        if (message.has_value()) {
            msgpack::adaptor::object_with_zone<IdT>()(o, message.value());
        } else {
            o.type = msgpack::type::NIL;
        }
    }
};

NS_END1
}
NS_END1
