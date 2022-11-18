/*
 * Copyright (c) 2022, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/Optional.h>
#include <LibWeb/WebDriver/Capabilities.h>
#include <LibWeb/WebDriver/TimeoutsConfiguration.h>

namespace Web::WebDriver {

// https://w3c.github.io/webdriver/#dfn-deserialize-as-a-page-load-strategy
static Response deserialize_as_a_page_load_strategy(JsonValue value)
{
    // 1. If value is not a string return an error with error code invalid argument.
    if (!value.is_string())
        return Error::from_code(ErrorCode::InvalidArgument, "Capability pageLoadStrategy must be a string"sv);

    // 2. If there is no entry in the table of page load strategies with keyword value return an error with error code invalid argument.
    if (!value.as_string().is_one_of("none"sv, "eager"sv, "normal"sv))
        return Error::from_code(ErrorCode::InvalidArgument, "Invalid pageLoadStrategy capability"sv);

    // 3. Return success with data value.
    return value;
}

// https://w3c.github.io/webdriver/#dfn-deserialize-as-an-unhandled-prompt-behavior
static Response deserialize_as_an_unhandled_prompt_behavior(JsonValue value)
{
    // 1. If value is not a string return an error with error code invalid argument.
    if (!value.is_string())
        return Error::from_code(ErrorCode::InvalidArgument, "Capability unhandledPromptBehavior must be a string"sv);

    // 2. If value is not present as a keyword in the known prompt handling approaches table return an error with error code invalid argument.
    if (!value.as_string().is_one_of("dismiss"sv, "accept"sv, "dismiss and notify"sv, "accept and notify"sv, "ignore"sv))
        return Error::from_code(ErrorCode::InvalidArgument, "Invalid pageLoadStrategy capability"sv);

    // 3. Return success with data value.
    return value;
}

// https://w3c.github.io/webdriver/#dfn-validate-capabilities
static ErrorOr<JsonObject, Error> validate_capabilities(JsonValue const& capability)
{
    // 1. If capability is not a JSON Object return an error with error code invalid argument.
    if (!capability.is_object())
        return Error::from_code(ErrorCode::InvalidArgument, "Capability is not an Object"sv);

    // 2. Let result be an empty JSON Object.
    JsonObject result;

    // 3. For each enumerable own property in capability, run the following substeps:
    TRY(capability.as_object().try_for_each_member([&](auto const& name, auto const& value) -> ErrorOr<void, Error> {
        // a. Let name be the name of the property.
        // b. Let value be the result of getting a property named name from capability.

        // c. Run the substeps of the first matching condition:
        JsonValue deserialized;

        // -> value is null
        if (value.is_null()) {
            // Let deserialized be set to null.
        }

        // -> name equals "acceptInsecureCerts"
        else if (name == "acceptInsecureCerts"sv) {
            // If value is not a boolean return an error with error code invalid argument. Otherwise, let deserialized be set to value
            if (!value.is_bool())
                return Error::from_code(ErrorCode::InvalidArgument, "Capability acceptInsecureCerts must be a boolean"sv);
            deserialized = value;
        }

        // -> name equals "browserName"
        // -> name equals "browserVersion"
        // -> name equals "platformName"
        else if (name.is_one_of("browserName"sv, "browser_version"sv, "platformName"sv)) {
            // If value is not a string return an error with error code invalid argument. Otherwise, let deserialized be set to value.
            if (!value.is_string())
                return Error::from_code(ErrorCode::InvalidArgument, String::formatted("Capability {} must be a string", name));
            deserialized = value;
        }

        // -> name equals "pageLoadStrategy"
        else if (name == "pageLoadStrategy"sv) {
            // Let deserialized be the result of trying to deserialize as a page load strategy with argument value.
            deserialized = TRY(deserialize_as_a_page_load_strategy(value));
        }

        // FIXME: -> name equals "proxy"
        // FIXME:     Let deserialized be the result of trying to deserialize as a proxy with argument value.

        // -> name equals "strictFileInteractability"
        else if (name == "strictFileInteractability"sv) {
            // If value is not a boolean return an error with error code invalid argument. Otherwise, let deserialized be set to value
            if (!value.is_bool())
                return Error::from_code(ErrorCode::InvalidArgument, "Capability strictFileInteractability must be a boolean"sv);
            deserialized = value;
        }

        // -> name equals "timeouts"
        else if (name == "timeouts"sv) {
            // Let deserialized be the result of trying to JSON deserialize as a timeouts configuration the value.
            auto timeouts = TRY(json_deserialize_as_a_timeouts_configuration(value));
            deserialized = JsonValue { timeouts_object(timeouts) };
        }

        // -> name equals "unhandledPromptBehavior"
        else if (name == "unhandledPromptBehavior"sv) {
            // Let deserialized be the result of trying to deserialize as an unhandled prompt behavior with argument value.
            deserialized = TRY(deserialize_as_an_unhandled_prompt_behavior(value));
        }

        // FIXME: -> name is the name of an additional WebDriver capability
        // FIXME:     Let deserialized be the result of trying to run the additional capability deserialization algorithm for the extension capability corresponding to name, with argument value.
        // FIXME: -> name is the key of an extension capability
        // FIXME:     If name is known to the implementation, let deserialized be the result of trying to deserialize value in an implementation-specific way. Otherwise, let deserialized be set to value.

        // -> The remote end is an endpoint node
        else {
            // Return an error with error code invalid argument.
            return Error::from_code(ErrorCode::InvalidArgument, String::formatted("Unrecognized capability: {}", name));
        }

        // d. If deserialized is not null, set a property on result with name name and value deserialized.
        if (!deserialized.is_null())
            result.set(name, move(deserialized));

        return {};
    }));

    // 4. Return success with data result.
    return result;
}

// https://w3c.github.io/webdriver/#dfn-merging-capabilities
static ErrorOr<JsonObject, Error> merge_capabilities(JsonObject const& primary, Optional<JsonObject const&> const& secondary)
{
    // 1. Let result be a new JSON Object.
    JsonObject result;

    // 2. For each enumerable own property in primary, run the following substeps:
    primary.for_each_member([&](auto const& name, auto const& value) {
        // a. Let name be the name of the property.
        // b. Let value be the result of getting a property named name from primary.

        // c. Set a property on result with name name and value value.
        result.set(name, value);
    });

    // 3. If secondary is undefined, return result.
    if (!secondary.has_value())
        return result;

    // 4. For each enumerable own property in secondary, run the following substeps:
    TRY(secondary->try_for_each_member([&](auto const& name, auto const& value) -> ErrorOr<void, Error> {
        // a. Let name be the name of the property.
        // b. Let value be the result of getting a property named name from secondary.

        // c. Let primary value be the result of getting the property name from primary.
        auto const* primary_value = primary.get_ptr(name);

        // d. If primary value is not undefined, return an error with error code invalid argument.
        if (primary_value != nullptr)
            return Error::from_code(ErrorCode::InvalidArgument, String::formatted("Unable to merge capability {}", name));

        // e. Set a property on result with name name and value value.
        result.set(name, value);
        return {};
    }));

    // 5. Return result.
    return result;
}

// https://w3c.github.io/webdriver/#dfn-capabilities-processing
Response process_capabilities(JsonValue const& parameters)
{
    if (!parameters.is_object())
        return Error::from_code(ErrorCode::InvalidArgument, "Session parameters is not an object"sv);

    // 1. Let capabilities request be the result of getting the property "capabilities" from parameters.
    //     a. If capabilities request is not a JSON Object, return error with error code invalid argument.
    auto const* capabilities_request_ptr = parameters.as_object().get_ptr("capabilities"sv);
    if (!capabilities_request_ptr || !capabilities_request_ptr->is_object())
        return Error::from_code(ErrorCode::InvalidArgument, "Capabilities is not an object"sv);

    auto const& capabilities_request = capabilities_request_ptr->as_object();

    // 2. Let required capabilities be the result of getting the property "alwaysMatch" from capabilities request.
    //     a. If required capabilities is undefined, set the value to an empty JSON Object.
    JsonObject required_capabilities;

    if (auto const* capability = capabilities_request.get_ptr("alwaysMatch"sv)) {
        // b. Let required capabilities be the result of trying to validate capabilities with argument required capabilities.
        required_capabilities = TRY(validate_capabilities(*capability));
    }

    // 3. Let all first match capabilities be the result of getting the property "firstMatch" from capabilities request.
    JsonArray all_first_match_capabilities;

    if (auto const* capabilities = capabilities_request.get_ptr("firstMatch"sv)) {
        // b. If all first match capabilities is not a JSON List with one or more entries, return error with error code invalid argument.
        if (!capabilities->is_array() || capabilities->as_array().is_empty())
            return Error::from_code(ErrorCode::InvalidArgument, "Capability firstMatch must be an array with at least one entry"sv);

        all_first_match_capabilities = capabilities->as_array();
    } else {
        // a. If all first match capabilities is undefined, set the value to a JSON List with a single entry of an empty JSON Object.
        all_first_match_capabilities.append(JsonObject {});
    }

    // 4. Let validated first match capabilities be an empty JSON List.
    JsonArray validated_first_match_capabilities;
    validated_first_match_capabilities.ensure_capacity(all_first_match_capabilities.size());

    // 5. For each first match capabilities corresponding to an indexed property in all first match capabilities:
    TRY(all_first_match_capabilities.try_for_each([&](auto const& first_match_capabilities) -> ErrorOr<void, Error> {
        // a. Let validated capabilities be the result of trying to validate capabilities with argument first match capabilities.
        auto validated_capabilities = TRY(validate_capabilities(first_match_capabilities));

        // b. Append validated capabilities to validated first match capabilities.
        validated_first_match_capabilities.append(move(validated_capabilities));
        return {};
    }));

    // 6. Let merged capabilities be an empty List.
    JsonArray merged_capabilities;
    merged_capabilities.ensure_capacity(validated_first_match_capabilities.size());

    // 7. For each first match capabilities corresponding to an indexed property in validated first match capabilities:
    TRY(validated_first_match_capabilities.try_for_each([&](auto const& first_match_capabilities) -> ErrorOr<void, Error> {
        // a. Let merged be the result of trying to merge capabilities with required capabilities and first match capabilities as arguments.
        auto merged = TRY(merge_capabilities(required_capabilities, first_match_capabilities.as_object()));

        // b. Append merged to merged capabilities.
        merged_capabilities.append(move(merged));
        return {};
    }));

    // FIXME: 8. For each capabilities corresponding to an indexed property in merged capabilities:
    // FIXME:    a. Let matched capabilities be the result of trying to match capabilities with capabilities as an argument.
    // FIXME:    b. If matched capabilities is not null, return success with data matched capabilities.

    // For now, we just assume the first validated capabilties object is a match.
    return merged_capabilities.take(0);

    // 9. Return success with data null.
}

}